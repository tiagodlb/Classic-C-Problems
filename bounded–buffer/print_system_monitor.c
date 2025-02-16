/**
 * Sistema de Fila de Impressão - Implementação usando Monitores
 *
 * Este programa implementa um sistema de fila de impressão usando o padrão Produtor-Consumidor
 * através do conceito de Monitores. Um monitor é uma estrutura de dados que encapsula tanto
 * os dados compartilhados quanto os mecanismos de sincronização necessários para acessá-los.
 *
 * O sistema simula um ambiente onde múltiplas aplicações (produtores) enviam documentos
 * para impressão, e múltiplas impressoras (consumidores) processam estes documentos.
 *
 * Características do Monitor:
 * - Encapsulamento de dados e sincronização
 * - Exclusão mútua automática
 * - Variáveis de condição para sincronização
 * - Gerenciamento de buffer circular
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdarg.h>

/**
 * Configurações do sistema
 */
#define BUFFER_SIZE 5      // Tamanho do buffer circular
#define NUM_PRODUCERS 3    // Número de threads produtoras (aplicações)
#define NUM_CONSUMERS 2    // Número de threads consumidoras (impressoras)
#define MAX_DOCUMENTS 10   // Máximo de documentos por produtor
#define MAX_TYPE_LENGTH 20 // Tamanho máximo do tipo do documento

/**
 * Estrutura que representa um documento na fila de impressão
 */
typedef struct
{
    int id;                     // Identificador único do documento
    char type[MAX_TYPE_LENGTH]; // Tipo do documento (ex: "Doc1", "Doc2")
    int size;                   // Tamanho do documento em KB
    int producer_id;            // ID do produtor que criou o documento
} Document;

/**
 * Monitor da fila de impressão
 *
 * Esta estrutura encapsula:
 * 1. Dados compartilhados (buffer e contadores)
 * 2. Mecanismos de sincronização (mutex e variáveis de condição)
 * 3. Estado do sistema
 */
typedef struct
{
    // Dados compartilhados
    Document buffer[BUFFER_SIZE]; // Buffer circular de documentos
    int count;                    // Número atual de documentos no buffer
    int in;                       // Índice para inserção
    int out;                      // Índice para remoção
    int active_producers;         // Número de produtores ativos

    // Mecanismos de sincronização
    pthread_mutex_t mutex;       // Mutex principal do monitor
    pthread_cond_t not_full;     // Condição: buffer não está cheio
    pthread_cond_t not_empty;    // Condição: buffer não está vazio
    pthread_mutex_t print_mutex; // Mutex para impressão thread-safe

    // Estado do sistema
    int should_stop; // Flag para controle de finalização
} PrintQueueMonitor;

// Instância global do monitor
PrintQueueMonitor print_queue;

/**
 * Inicializa o monitor e seus mecanismos de sincronização
 *
 * @param m Ponteiro para o monitor
 */
void monitor_init(PrintQueueMonitor *m)
{
    // Inicializa contadores
    m->count = 0;
    m->in = 0;
    m->out = 0;
    m->active_producers = NUM_PRODUCERS;
    m->should_stop = 0;

    // Inicializa mecanismos de sincronização
    pthread_mutex_init(&m->mutex, NULL);
    pthread_mutex_init(&m->print_mutex, NULL);
    pthread_cond_init(&m->not_full, NULL);
    pthread_cond_init(&m->not_empty, NULL);
}

/**
 * Libera recursos alocados pelo monitor
 *
 * @param m Ponteiro para o monitor
 */
void monitor_destroy(PrintQueueMonitor *m)
{
    pthread_mutex_destroy(&m->mutex);
    pthread_mutex_destroy(&m->print_mutex);
    pthread_cond_destroy(&m->not_full);
    pthread_cond_destroy(&m->not_empty);
}

/**
 * Função thread-safe para impressão de mensagens
 *
 * @param m Ponteiro para o monitor
 * @param format String de formato
 * @param ... Argumentos variáveis
 */
void monitor_print(PrintQueueMonitor *m, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    pthread_mutex_lock(&m->print_mutex);
    vprintf(format, args);
    fflush(stdout);
    pthread_mutex_unlock(&m->print_mutex);

    va_end(args);
}

/**
 * Insere um documento no buffer do monitor
 *
 * Esta função bloqueia se o buffer estiver cheio até que haja espaço disponível.
 * Implementa a semântica do monitor usando mutex e variável de condição.
 *
 * @param m Ponteiro para o monitor
 * @param doc Documento a ser inserido
 */
void monitor_insert(PrintQueueMonitor *m, Document doc)
{
    pthread_mutex_lock(&m->mutex);

    // Aguarda espaço disponível no buffer
    while (m->count == BUFFER_SIZE && !m->should_stop)
    {
        pthread_cond_wait(&m->not_full, &m->mutex);
    }

    if (m->should_stop)
    {
        pthread_mutex_unlock(&m->mutex);
        return;
    }

    // Insere documento e atualiza estado
    m->buffer[m->in] = doc;
    monitor_print(m, "[Produtor %d] Adicionou documento %d (%s, %dKB) na posição %d\n",
                  doc.producer_id, doc.id, doc.type, doc.size, m->in);

    m->in = (m->in + 1) % BUFFER_SIZE;
    m->count++;

    pthread_cond_signal(&m->not_empty);
    pthread_mutex_unlock(&m->mutex);
}

/**
 * Remove um documento do buffer do monitor
 *
 * Esta função bloqueia se o buffer estiver vazio até que haja um documento disponível.
 *
 * @param m Ponteiro para o monitor
 * @param doc Ponteiro para armazenar o documento removido
 * @return 1 se um documento foi removido, 0 caso contrário
 */
int monitor_remove(PrintQueueMonitor *m, Document *doc)
{
    pthread_mutex_lock(&m->mutex);

    while (m->count == 0 && !m->should_stop)
    {
        if (m->active_producers == 0)
        {
            pthread_mutex_unlock(&m->mutex);
            return 0;
        }
        pthread_cond_wait(&m->not_empty, &m->mutex);
    }

    if (m->should_stop && m->count == 0)
    {
        pthread_mutex_unlock(&m->mutex);
        return 0;
    }

    *doc = m->buffer[m->out];
    m->out = (m->out + 1) % BUFFER_SIZE;
    m->count--;

    pthread_cond_signal(&m->not_full);
    pthread_mutex_unlock(&m->mutex);

    return 1;
}

/**
 * Thread produtora - simula uma aplicação gerando documentos
 *
 * @param arg Ponteiro para o ID do produtor
 * @return NULL
 */
void *producer(void *arg)
{
    int producer_id = *(int *)arg;
    int docs_produced = 0;

    while (docs_produced < MAX_DOCUMENTS && !print_queue.should_stop)
    {
        Document doc = {
            .id = (producer_id * MAX_DOCUMENTS) + docs_produced,
            .size = rand() % 100 + 1,
            .producer_id = producer_id};
        snprintf(doc.type, MAX_TYPE_LENGTH, "Doc%d", producer_id);

        monitor_insert(&print_queue, doc);

        docs_produced++;
        usleep(rand() % 500000); // Simula tempo de produção
    }

    pthread_mutex_lock(&print_queue.mutex);
    print_queue.active_producers--;
    pthread_cond_broadcast(&print_queue.not_empty);
    pthread_mutex_unlock(&print_queue.mutex);

    monitor_print(&print_queue, "[Produtor %d] Finalizou após produzir %d documentos\n",
                  producer_id, docs_produced);
    return NULL;
}

/**
 * Thread consumidora - simula uma impressora
 *
 * @param arg Ponteiro para o ID do consumidor
 * @return NULL
 */
void *consumer(void *arg)
{
    int consumer_id = *(int *)arg;
    int docs_consumed = 0;
    Document doc;

    while (!print_queue.should_stop || print_queue.count > 0)
    {
        if (monitor_remove(&print_queue, &doc))
        {
            monitor_print(&print_queue,
                          "[Consumidor %d] Imprimindo documento %d (%s, %dKB)\n",
                          consumer_id, doc.id, doc.type, doc.size);

            docs_consumed++;
            usleep(doc.size * 10000); // Simula tempo de impressão
        }
        else if (print_queue.active_producers == 0)
        {
            break;
        }
    }

    monitor_print(&print_queue, "[Consumidor %d] Finalizou após consumir %d documentos\n",
                  consumer_id, docs_consumed);
    return NULL;
}

/**
 * Função principal
 * Inicializa o sistema, cria threads e gerencia o ciclo de vida
 */
int main()
{
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    int producer_ids[NUM_PRODUCERS];
    int consumer_ids[NUM_CONSUMERS];
    int i;

    monitor_init(&print_queue);

    // Cria threads produtoras
    for (i = 0; i < NUM_PRODUCERS; i++)
    {
        producer_ids[i] = i + 1;
        if (pthread_create(&producers[i], NULL, producer, &producer_ids[i]) != 0)
        {
            fprintf(stderr, "Erro ao criar produtor %d\n", i + 1);
            print_queue.should_stop = 1;
            return 1;
        }
    }

    // Cria threads consumidoras
    for (i = 0; i < NUM_CONSUMERS; i++)
    {
        consumer_ids[i] = i + 1;
        if (pthread_create(&consumers[i], NULL, consumer, &consumer_ids[i]) != 0)
        {
            fprintf(stderr, "Erro ao criar consumidor %d\n", i + 1);
            print_queue.should_stop = 1;
            return 1;
        }
    }

    // Aguarda conclusão das threads
    for (i = 0; i < NUM_PRODUCERS; i++)
    {
        pthread_join(producers[i], NULL);
    }

    for (i = 0; i < NUM_CONSUMERS; i++)
    {
        pthread_join(consumers[i], NULL);
    }

    monitor_destroy(&print_queue);

    printf("Sistema finalizado com sucesso\n");
    return 0;
}
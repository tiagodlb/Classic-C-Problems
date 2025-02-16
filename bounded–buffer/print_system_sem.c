/**
 * Sistema de Fila de Impressão - Implementação usando Semáforos
 *
 * Este programa implementa um sistema de fila de impressão usando o padrão Produtor-Consumidor.
 * Utiliza semáforos para sincronização entre múltiplos produtores (aplicações) e
 * consumidores (impressoras). O sistema usa um buffer circular para armazenar os documentos.
 *
 * Características principais:
 * - Buffer circular com tamanho fixo
 * - Múltiplos produtores e consumidores
 * - Sincronização usando semáforos POSIX
 * - Simulação de tempos variáveis de produção e consumo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
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
 * Buffer circular e variáveis de controle
 */
Document buffer[BUFFER_SIZE]; // Buffer circular para armazenar documentos
int in = 0;                   // Índice para inserção no buffer
int out = 0;                  // Índice para remoção do buffer

/**
 * Semáforos para controle de sincronização
 */
sem_t empty;       // Controla número de espaços vazios no buffer
sem_t full;        // Controla número de espaços ocupados no buffer
sem_t mutex;       // Protege acesso à região crítica (buffer)
sem_t print_mutex; // Protege operações de impressão no console

/**
 * Flag global para controle de finalização do sistema
 */
volatile int should_stop = 0;

/**
 * Função thread-safe para impressão de mensagens no console
 * Usa semáforo para garantir que apenas uma thread imprima por vez
 *
 * @param format String de formato (igual ao printf)
 * @param ... Argumentos variáveis para o formato
 */
void safe_print(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    sem_wait(&print_mutex); // Obtém acesso exclusivo ao console
    vprintf(format, args);
    fflush(stdout);
    sem_post(&print_mutex); // Libera acesso ao console

    va_end(args);
}

/**
 * Função executada pelas threads produtoras
 * Simula aplicações gerando documentos para impressão
 *
 * @param arg Ponteiro para o ID do produtor
 * @return NULL
 */
void *producer(void *arg)
{
    int producer_id = *(int *)arg;
    int docs_produced = 0;

    while (docs_produced < MAX_DOCUMENTS && !should_stop)
    {
        // Cria novo documento com dados simulados
        Document doc = {
            .id = (producer_id * MAX_DOCUMENTS) + docs_produced,
            .size = rand() % 100 + 1,
            .producer_id = producer_id};
        snprintf(doc.type, MAX_TYPE_LENGTH, "Doc%d", producer_id);

        sem_wait(&empty); // Aguarda espaço vazio no buffer
        sem_wait(&mutex); // Entra na região crítica

        // Adiciona documento ao buffer
        buffer[in] = doc;
        safe_print("[Produtor %d] Adicionou documento %d (%s, %dKB) na posição %d\n",
                   producer_id, doc.id, doc.type, doc.size, in);

        in = (in + 1) % BUFFER_SIZE; // Atualiza índice de inserção

        sem_post(&mutex); // Sai da região crítica
        sem_post(&full);  // Sinaliza item produzido

        docs_produced++;
        usleep(rand() % 500000); // Simula tempo variável de produção (0-500ms)
    }

    safe_print("[Produtor %d] Finalizou após produzir %d documentos\n",
               producer_id, docs_produced);
    return NULL;
}

/**
 * Função executada pelas threads consumidoras
 * Simula impressoras processando documentos
 *
 * @param arg Ponteiro para o ID do consumidor
 * @return NULL
 */
void *consumer(void *arg)
{
    int consumer_id = *(int *)arg;
    int docs_consumed = 0;

    while (!should_stop)
    {
        sem_wait(&full);  // Aguarda documento disponível
        sem_wait(&mutex); // Entra na região crítica

        // Remove documento do buffer
        Document doc = buffer[out];
        safe_print("[Consumidor %d] Imprimindo documento %d (%s, %dKB) da posição %d\n",
                   consumer_id, doc.id, doc.type, doc.size, out);

        out = (out + 1) % BUFFER_SIZE; // Atualiza índice de remoção
        docs_consumed++;

        sem_post(&mutex); // Sai da região crítica
        sem_post(&empty); // Sinaliza espaço livre

        // Simula tempo de impressão proporcional ao tamanho do documento
        usleep(doc.size * 10000);
    }

    safe_print("[Consumidor %d] Finalizou após consumir %d documentos\n",
               consumer_id, docs_consumed);
    return NULL;
}

/**
 * Inicializa todos os semáforos necessários
 *
 * @return 0 em caso de sucesso, -1 em caso de erro
 */
int init_semaphores()
{
    // Inicializa semáforo para espaços vazios
    if (sem_init(&empty, 0, BUFFER_SIZE) != 0)
    {
        printf("Erro ao inicializar semáforo empty\n");
        return -1;
    }

    // Inicializa semáforo para documentos disponíveis
    if (sem_init(&full, 0, 0) != 0)
    {
        printf("Erro ao inicializar semáforo full\n");
        sem_destroy(&empty);
        return -1;
    }

    // Inicializa mutex para proteção do buffer
    if (sem_init(&mutex, 0, 1) != 0)
    {
        printf("Erro ao inicializar semáforo mutex\n");
        sem_destroy(&empty);
        sem_destroy(&full);
        return -1;
    }

    // Inicializa mutex para proteção da impressão
    if (sem_init(&print_mutex, 0, 1) != 0)
    {
        printf("Erro ao inicializar semáforo print_mutex\n");
        sem_destroy(&empty);
        sem_destroy(&full);
        sem_destroy(&mutex);
        return -1;
    }

    return 0;
}

/**
 * Libera recursos alocados pelos semáforos
 */
void destroy_semaphores()
{
    sem_destroy(&empty);
    sem_destroy(&full);
    sem_destroy(&mutex);
    sem_destroy(&print_mutex);
}

/**
 * Função principal
 * Inicializa o sistema, cria threads e gerencia ciclo de vida
 */
int main()
{
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    int producer_ids[NUM_PRODUCERS];
    int consumer_ids[NUM_CONSUMERS];
    int i;

    // Inicializa sistema de semáforos
    if (init_semaphores() != 0)
    {
        printf("Falha na inicialização dos semáforos\n");
        return 1;
    }

    // Cria threads produtoras
    for (i = 0; i < NUM_PRODUCERS; i++)
    {
        producer_ids[i] = i + 1;
        if (pthread_create(&producers[i], NULL, producer, &producer_ids[i]) != 0)
        {
            printf("Erro ao criar produtor %d\n", i + 1);
            should_stop = 1;
            destroy_semaphores();
            return 1;
        }
    }

    // Cria threads consumidoras
    for (i = 0; i < NUM_CONSUMERS; i++)
    {
        consumer_ids[i] = i + 1;
        if (pthread_create(&consumers[i], NULL, consumer, &consumer_ids[i]) != 0)
        {
            printf("Erro ao criar consumidor %d\n", i + 1);
            should_stop = 1;
            destroy_semaphores();
            return 1;
        }
    }

    // Aguarda produtores finalizarem
    for (i = 0; i < NUM_PRODUCERS; i++)
    {
        pthread_join(producers[i], NULL);
    }

    // Sinaliza finalização para consumidores
    should_stop = 1;

    // Libera consumidores que possam estar bloqueados
    for (i = 0; i < NUM_CONSUMERS; i++)
    {
        sem_post(&full);
    }

    // Aguarda consumidores finalizarem
    for (i = 0; i < NUM_CONSUMERS; i++)
    {
        pthread_join(consumers[i], NULL);
    }

    // Libera recursos
    destroy_semaphores();

    printf("Sistema finalizado com sucesso\n");
    return 0;
}
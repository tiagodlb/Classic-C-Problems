/**
 * Sistema de Fila de Impressão com Mutex e Variáveis de Condição
 *
 * Este programa implementa um sistema de fila de impressão thread-safe usando o padrão Produtor-Consumidor.
 * Demonstra a sincronização entre múltiplas threads usando mutexes e variáveis de condição.
 * O sistema simula um ambiente de impressão onde múltiplas aplicações (produtores) enviam
 * documentos para serem impressos por múltiplas impressoras (consumidores).
 *
 * Características Principais:
 * - Implementação de buffer circular para armazenamento de documentos
 * - Submissão e processamento thread-safe de documentos
 * - Sincronização dinâmica entre produtores e consumidores
 * - Desligamento controlado do sistema
 * - Tratamento de erros e gerenciamento de recursos
 *
 * Mecanismos de Sincronização:
 * - Mutex para proteção de recursos compartilhados
 * - Variáveis de condição para sinalização entre threads
 * - Coordenação entre Produtores e Consumidores
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

/**
 * Constantes de Configuração do Sistema
 *
 * Estas constantes definem os parâmetros operacionais do sistema de fila de impressão.
 * Modifique estes valores para ajustar o comportamento do sistema.
 */
#define BUFFER_SIZE 5      // Tamanho do buffer circular
#define NUM_PRODUCERS 3    // Número de threads produtoras (aplicações)
#define NUM_CONSUMERS 2    // Número de threads consumidoras (impressoras)
#define MAX_DOCUMENTS 10   // Máximo de documentos por produtor
#define MAX_TYPE_LENGTH 20 // Tamanho máximo para o tipo do documento

/**
 * Códigos de Erro do Sistema
 *
 * Estes códigos são retornados por várias funções do sistema para indicar
 * sucesso ou condições específicas de falha.
 */
#define PRINT_SUCCESS 0     // Operação concluída com sucesso
#define PRINT_ERR_MUTEX -1  // Falha na inicialização/operação do mutex
#define PRINT_ERR_THREAD -2 // Falha na criação/operação da thread
#define PRINT_ERR_COND -3   // Falha na inicialização/operação da variável de condição

/**
 * Estrutura do Documento
 *
 * Representa um trabalho de impressão no sistema. Cada documento contém
 * metadados sobre o trabalho de impressão e sua origem.
 */
typedef struct
{
    int id;                     // Identificador único do documento
    char type[MAX_TYPE_LENGTH]; // Tipo do documento (ex: "PDF", "DOC")
    int size;                   // Tamanho do documento em KB
    int producer_id;            // ID da aplicação produtora
} Document;

/**
 * Estrutura da Fila de Impressão
 *
 * Estrutura de dados principal que gerencia o sistema de fila de impressão. Contém:
 * 1. O buffer circular e seus índices de gerenciamento
 * 2. Primitivas de sincronização
 * 3. Informações de estado do sistema
 */
typedef struct
{
    // Gerenciamento do Buffer
    Document buffer[BUFFER_SIZE]; // Buffer circular que armazena documentos
    int in;                       // Índice para próxima inserção (produtor)
    int out;                      // Índice para próxima remoção (consumidor)
    int count;                    // Número atual de documentos no buffer

    // Primitivas de Sincronização
    pthread_mutex_t mutex;    // Protege acesso aos recursos compartilhados
    pthread_cond_t not_full;  // Sinaliza quando o buffer não está cheio
    pthread_cond_t not_empty; // Sinaliza quando o buffer não está vazio

    // Estado do Sistema
    int active_producers; // Número de threads produtoras ativas
    int should_stop;      // Flag para desligamento do sistema
} PrintQueue;

// Instância global da fila de impressão
PrintQueue print_queue = {
    .in = 0,
    .out = 0,
    .count = 0,
    .active_producers = 0,
    .should_stop = 0};

/**
 * Inicializa o sistema de fila de impressão
 *
 * Configura as primitivas de sincronização e inicializa o estado do sistema.
 * Deve ser chamada antes de qualquer outra operação na fila de impressão.
 *
 * @return PRINT_SUCCESS em caso de sucesso, código de erro em caso de falha
 */
int init_print_queue(void)
{
    // Inicializa o mutex principal
    if (pthread_mutex_init(&print_queue.mutex, NULL) != 0)
    {
        fprintf(stderr, "Falha ao inicializar mutex: %s\n", strerror(errno));
        return PRINT_ERR_MUTEX;
    }

    // Inicializa variável de condição para not_full
    if (pthread_cond_init(&print_queue.not_full, NULL) != 0)
    {
        pthread_mutex_destroy(&print_queue.mutex);
        fprintf(stderr, "Falha ao inicializar condição not_full: %s\n", strerror(errno));
        return PRINT_ERR_COND;
    }

    // Inicializa variável de condição para not_empty
    if (pthread_cond_init(&print_queue.not_empty, NULL) != 0)
    {
        pthread_mutex_destroy(&print_queue.mutex);
        pthread_cond_destroy(&print_queue.not_full);
        fprintf(stderr, "Falha ao inicializar condição not_empty: %s\n", strerror(errno));
        return PRINT_ERR_COND;
    }

    return PRINT_SUCCESS;
}

/**
 * Libera recursos da fila de impressão
 *
 * Libera todos os recursos do sistema e limpa as primitivas de sincronização.
 * Deve ser chamada quando o sistema está sendo desligado.
 */
void cleanup_print_queue(void)
{
    pthread_mutex_destroy(&print_queue.mutex);
    pthread_cond_destroy(&print_queue.not_full);
    pthread_cond_destroy(&print_queue.not_empty);
}

/**
 * Função da Thread Produtora
 *
 * Simula uma aplicação enviando documentos para a fila de impressão.
 * Cada produtor cria uma série de documentos e os adiciona à fila.
 *
 * Fluxo do Produtor:
 * 1. Cria um novo documento com ID único e tamanho aleatório
 * 2. Aguarda se o buffer estiver cheio
 * 3. Adiciona o documento ao buffer quando houver espaço
 * 4. Sinaliza aos consumidores que um novo documento está disponível
 *
 * @param arg Ponteiro para o ID do produtor (int)
 * @return NULL
 */
void *producer(void *arg)
{
    int producer_id = *(int *)arg;
    int docs_produced = 0;

    // Registra como produtor ativo
    pthread_mutex_lock(&print_queue.mutex);
    print_queue.active_producers++;
    pthread_mutex_unlock(&print_queue.mutex);

    // Loop principal de produção
    while (docs_produced < MAX_DOCUMENTS && !print_queue.should_stop)
    {
        // Cria novo documento com propriedades simuladas
        Document doc = {
            .id = (producer_id * MAX_DOCUMENTS) + docs_produced,
            .size = rand() % 100 + 1,
            .producer_id = producer_id};
        snprintf(doc.type, MAX_TYPE_LENGTH, "Doc%d", producer_id);

        pthread_mutex_lock(&print_queue.mutex);

        // Aguarda enquanto o buffer estiver cheio
        while (print_queue.count == BUFFER_SIZE && !print_queue.should_stop)
        {
            pthread_cond_wait(&print_queue.not_full, &print_queue.mutex);
        }

        if (print_queue.should_stop)
        {
            pthread_mutex_unlock(&print_queue.mutex);
            break;
        }

        // Adiciona documento ao buffer
        print_queue.buffer[print_queue.in] = doc;
        printf("[Produtor %d] Adicionou documento %d (%s, %dKB) na posição %d\n",
               producer_id, doc.id, doc.type, doc.size, print_queue.in);

        // Atualiza estado do buffer
        print_queue.in = (print_queue.in + 1) % BUFFER_SIZE;
        print_queue.count++;

        pthread_cond_signal(&print_queue.not_empty);
        pthread_mutex_unlock(&print_queue.mutex);

        docs_produced++;
        usleep(rand() % 500000); // Simula tempo variável de criação de documento
    }

    // Remove registro do produtor e sinaliza conclusão
    pthread_mutex_lock(&print_queue.mutex);
    print_queue.active_producers--;
    if (print_queue.active_producers == 0)
    {
        pthread_cond_broadcast(&print_queue.not_empty);
    }
    pthread_mutex_unlock(&print_queue.mutex);

    printf("[Produtor %d] Finalizou a produção de documentos\n", producer_id);
    return NULL;
}

/**
 * Função da Thread Consumidora
 *
 * Simula uma impressora processando documentos da fila.
 * Cada consumidor continuamente remove e processa documentos até
 * que o sistema seja desligado ou não haja mais documentos a serem produzidos.
 *
 * Fluxo do Consumidor:
 * 1. Aguarda se o buffer estiver vazio
 * 2. Remove um documento quando disponível
 * 3. Processa (imprime) o documento
 * 4. Sinaliza aos produtores que há espaço disponível
 *
 * @param arg Ponteiro para o ID do consumidor (int)
 * @return NULL
 */
void *consumer(void *arg)
{
    int consumer_id = *(int *)arg;

    while (!print_queue.should_stop)
    {
        pthread_mutex_lock(&print_queue.mutex);

        // Aguarda por documentos disponíveis
        while (print_queue.count == 0)
        {
            if (print_queue.active_producers == 0)
            {
                pthread_mutex_unlock(&print_queue.mutex);
                printf("[Consumidor %d] Não há mais documentos para imprimir, encerrando\n", consumer_id);
                return NULL;
            }
            pthread_cond_wait(&print_queue.not_empty, &print_queue.mutex);
        }

        // Processa documento
        Document doc = print_queue.buffer[print_queue.out];
        printf("[Consumidor %d] Imprimindo documento %d (%s, %dKB) da posição %d\n",
               consumer_id, doc.id, doc.type, doc.size, print_queue.out);

        // Atualiza estado do buffer
        print_queue.out = (print_queue.out + 1) % BUFFER_SIZE;
        print_queue.count--;

        pthread_cond_signal(&print_queue.not_full);
        pthread_mutex_unlock(&print_queue.mutex);

        // Simula tempo de impressão proporcional ao tamanho do documento
        usleep(doc.size * 10000);
    }

    return NULL;
}

/**
 * Função Principal
 *
 * Inicializa o sistema, cria threads produtoras e consumidoras,
 * aguarda conclusão e realiza limpeza.
 *
 * Ciclo de Vida do Sistema:
 * 1. Inicializa fila de impressão e primitivas de sincronização
 * 2. Cria threads produtoras e consumidoras
 * 3. Aguarda conclusão de todas as threads
 * 4. Limpa recursos
 *
 * @return EXIT_SUCCESS em caso de execução bem-sucedida, EXIT_FAILURE caso contrário
 */
int main()
{
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    int producer_ids[NUM_PRODUCERS];
    int consumer_ids[NUM_CONSUMERS];
    int ret;

    // Inicializa sistema
    if ((ret = init_print_queue()) != PRINT_SUCCESS)
    {
        fprintf(stderr, "Falha ao inicializar fila de impressão: %d\n", ret);
        return EXIT_FAILURE;
    }

    // Cria threads produtoras
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        producer_ids[i] = i + 1;
        if (pthread_create(&producers[i], NULL, producer, &producer_ids[i]) != 0)
        {
            fprintf(stderr, "Falha ao criar thread produtora %d: %s\n", i, strerror(errno));
            print_queue.should_stop = 1;
            return EXIT_FAILURE;
        }
    }

    // Cria threads consumidoras
    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        consumer_ids[i] = i + 1;
        if (pthread_create(&consumers[i], NULL, consumer, &consumer_ids[i]) != 0)
        {
            fprintf(stderr, "Falha ao criar thread consumidora %d: %s\n", i, strerror(errno));
            print_queue.should_stop = 1;
            return EXIT_FAILURE;
        }
    }

    // Aguarda conclusão das threads
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        pthread_join(producers[i], NULL);
    }
    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        pthread_join(consumers[i], NULL);
    }

    // Limpa recursos
    cleanup_print_queue();
    printf("Sistema de fila de impressão finalizado com sucesso\n");

    return EXIT_SUCCESS;
}
/**
 * Sistema de Banco de Dados de E-commerce - Implementação com Semáforos
 *
 * Este programa implementa um sistema de controle de acesso ao catálogo de produtos
 * de um e-commerce usando o problema clássico dos leitores/escritores. O sistema
 * permite que múltiplos clientes (leitores) consultem o catálogo simultaneamente,
 * enquanto garante acesso exclusivo para funcionários (escritores) que precisam
 * atualizar informações de produtos.
 *
 * Mecanismos de Sincronização:
 * - Semáforos para controle de acesso
 * - Prioridade para leitores (clientes)
 * - Exclusão mútua para escritores (funcionários)
 *
 * Componentes do Sistema:
 * - Catálogo de produtos compartilhado
 * - Clientes consultando produtos
 * - Funcionários atualizando preços e estoque
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>

/**
 * Constantes de Configuração do Sistema
 */
#define NUM_READERS 5    // Número de clientes simultâneos no sistema
#define NUM_WRITERS 2    // Número de funcionários simultâneos no sistema
#define NUM_READS 5      // Número de consultas que cada cliente fará
#define NUM_WRITES 3     // Número de atualizações que cada funcionário fará
#define MAX_PRODUCTS 100 // Capacidade máxima do catálogo de produtos

/**
 * Estrutura do Produto
 *
 * Representa um item no catálogo com suas informações básicas.
 * Cada produto possui um identificador único, preço e quantidade em estoque.
 */
typedef struct
{
    int id;      // Identificador único do produto
    float price; // Preço atual do produto em reais
    int stock;   // Quantidade disponível em estoque
} Product;

/**
 * Estrutura do Catálogo
 *
 * Mantém o catálogo de produtos e os mecanismos de sincronização necessários
 * para garantir acesso seguro em um ambiente multi-thread.
 */
typedef struct
{
    Product products[MAX_PRODUCTS]; // Array de produtos do catálogo
    int num_readers;                // Contador de leitores ativos

    sem_t mutex;       // Semáforo para proteção geral
    sem_t write_mutex; // Semáforo para exclusão mútua de escritores
    sem_t read_mutex;  // Semáforo para proteção do contador de leitores
} Catalog;

// Instância global do catálogo
Catalog catalog = {
    .num_readers = 0};

/**
 * Inicializa o catálogo e seus mecanismos de sincronização
 *
 * Esta função:
 * 1. Inicializa os semáforos necessários
 * 2. Popula o catálogo com dados iniciais simulados
 * 3. Configura o estado inicial do sistema
 */
void init_catalog()
{
    // Inicializa semáforos como binários (valor inicial 1)
    sem_init(&catalog.mutex, 0, 1);       // Semáforo para proteção geral
    sem_init(&catalog.write_mutex, 0, 1); // Semáforo para controle de escrita
    sem_init(&catalog.read_mutex, 0, 1);  // Semáforo para controle de leitura

    // Popula o catálogo com produtos simulados
    for (int i = 0; i < MAX_PRODUCTS; i++)
    {
        catalog.products[i].id = i + 1;
        catalog.products[i].price = 10.0 + (rand() % 1000); // Preço entre 10 e 1010
        catalog.products[i].stock = rand() % 50;            // Estoque entre 0 e 49
    }
}

/**
 * Thread Leitora - Simula um Cliente
 *
 * Implementa o comportamento de um cliente consultando produtos no catálogo.
 * Utiliza o protocolo de leitores para garantir acesso seguro e simultâneo
 * com outros leitores.
 *
 * Protocolo de Leitura:
 * 1. Solicita acesso para leitura
 * 2. Incrementa contador de leitores
 * 3. Primeiro leitor bloqueia escritores
 * 4. Realiza a leitura
 * 5. Decrementa contador de leitores
 * 6. Último leitor libera escritores
 *
 * @param arg Ponteiro para o ID do cliente
 * @return NULL
 */
void *reader(void *arg)
{
    int id = *(int *)arg;

    for (int i = 0; i < NUM_READS; i++)
    {
        // Protocolo de entrada para leitura
        sem_wait(&catalog.read_mutex);
        catalog.num_readers++;
        if (catalog.num_readers == 1)
        {
            sem_wait(&catalog.write_mutex); // Primeiro leitor bloqueia escritores
        }
        sem_post(&catalog.read_mutex);

        // Seção crítica - Consulta do produto
        int product_id = rand() % MAX_PRODUCTS;
        Product product = catalog.products[product_id];
        printf("Cliente %d consultando produto %d: Preço = R$%.2f, Estoque = %d\n",
               id, product.id, product.price, product.stock);

        usleep(rand() % 500000); // Simula tempo de consulta (0-500ms)

        // Protocolo de saída da leitura
        sem_wait(&catalog.read_mutex);
        catalog.num_readers--;
        if (catalog.num_readers == 0)
        {
            sem_post(&catalog.write_mutex); // Último leitor libera escritores
        }
        sem_post(&catalog.read_mutex);

        usleep(rand() % 1000000); // Intervalo entre consultas (0-1s)
    }

    printf("Cliente %d finalizou suas consultas\n", id);
    return NULL;
}

/**
 * Thread Escritora - Simula um Funcionário
 *
 * Implementa o comportamento de um funcionário atualizando produtos no catálogo.
 * Utiliza exclusão mútua para garantir acesso exclusivo durante atualizações.
 *
 * Protocolo de Escrita:
 * 1. Solicita acesso exclusivo
 * 2. Realiza as atualizações
 * 3. Libera o acesso
 *
 * @param arg Ponteiro para o ID do funcionário
 * @return NULL
 */
void *writer(void *arg)
{
    int id = *(int *)arg;

    for (int i = 0; i < NUM_WRITES; i++)
    {
        // Protocolo de entrada para escrita
        sem_wait(&catalog.write_mutex);

        // Seção crítica - Atualização do produto
        int product_id = rand() % MAX_PRODUCTS;
        float price_change = (rand() % 20) - 10; // Variação de preço entre -10% e +10%
        int stock_change = (rand() % 10) - 3;    // Variação de estoque entre -3 e +6

        Product *product = &catalog.products[product_id];
        product->price *= (1 + price_change / 100.0);
        product->stock = product->stock + stock_change;
        if (product->stock < 0)
            product->stock = 0;

        printf("Funcionário %d atualizando produto %d: Novo preço = R$%.2f, Novo estoque = %d\n",
               id, product->id, product->price, product->stock);

        usleep(rand() % 1000000); // Simula tempo de atualização (0-1s)

        // Protocolo de saída da escrita
        sem_post(&catalog.write_mutex);

        usleep(rand() % 2000000); // Intervalo entre atualizações (0-2s)
    }

    printf("Funcionário %d finalizou suas atualizações\n", id);
    return NULL;
}

/**
 * Libera recursos alocados pelo sistema
 *
 * Realiza a limpeza adequada dos semáforos e outros recursos
 * que foram alocados durante a execução do programa.
 */
void cleanup_catalog()
{
    sem_destroy(&catalog.mutex);
    sem_destroy(&catalog.write_mutex);
    sem_destroy(&catalog.read_mutex);
}

/**
 * Função Principal
 *
 * Inicializa o sistema, cria as threads de leitores e escritores,
 * aguarda sua conclusão e realiza a limpeza dos recursos.
 *
 * Fluxo de Execução:
 * 1. Inicializa o catálogo e semáforos
 * 2. Cria threads de clientes e funcionários
 * 3. Aguarda conclusão das operações
 * 4. Limpa recursos e finaliza
 *
 * @return 0 em caso de sucesso, 1 em caso de erro
 */
int main()
{
    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    int reader_ids[NUM_READERS];
    int writer_ids[NUM_WRITERS];

    init_catalog();

    // Cria threads de clientes
    for (int i = 0; i < NUM_READERS; i++)
    {
        reader_ids[i] = i + 1;
        if (pthread_create(&readers[i], NULL, reader, &reader_ids[i]) != 0)
        {
            fprintf(stderr, "Erro ao criar thread de cliente %d\n", i);
            cleanup_catalog();
            return 1;
        }
    }

    // Cria threads de funcionários
    for (int i = 0; i < NUM_WRITERS; i++)
    {
        writer_ids[i] = i + 1;
        if (pthread_create(&writers[i], NULL, writer, &writer_ids[i]) != 0)
        {
            fprintf(stderr, "Erro ao criar thread de funcionário %d\n", i);
            cleanup_catalog();
            return 1;
        }
    }

    // Aguarda conclusão das threads
    for (int i = 0; i < NUM_READERS; i++)
    {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < NUM_WRITERS; i++)
    {
        pthread_join(writers[i], NULL);
    }

    cleanup_catalog();
    printf("Sistema finalizado com sucesso\n");
    return 0;
}
/**
 * Sistema de Banco de Dados de E-commerce - Implementação com Mutex
 *
 * Implementa o problema clássico dos leitores/escritores utilizando mutex em um
 * contexto de e-commerce. O sistema permite que múltiplos clientes (leitores)
 * consultem o catálogo simultaneamente, enquanto garante acesso exclusivo para
 * funcionários (escritores) realizarem atualizações.
 *
 * Características do Sistema:
 * - Múltiplos clientes podem ler simultaneamente
 * - Funcionários têm acesso exclusivo para atualizações
 * - Prioridade para leitores (clientes)
 * - Controle de concorrência usando mutex
 *
 * Funcionamento:
 * 1. Clientes podem consultar produtos simultaneamente
 * 2. Funcionários atualizam preços e estoque com exclusão mútua
 * 3. Sistema prioriza experiência do cliente (leitores)
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/**
 * Constantes de Configuração do Sistema
 */
#define NUM_READERS 5    // Número de clientes simultâneos
#define NUM_WRITERS 2    // Número de funcionários simultâneos
#define NUM_READS 5      // Consultas por cliente
#define NUM_WRITES 3     // Atualizações por funcionário
#define MAX_PRODUCTS 100 // Capacidade do catálogo

/**
 * Estrutura do Produto
 *
 * Representa um item no catálogo com suas informações básicas.
 * Cada produto mantém informações de identificação, preço e estoque.
 */
typedef struct
{
    int id;      // Identificador único do produto
    float price; // Preço atual em reais
    int stock;   // Quantidade em estoque
} Product;

/**
 * Estrutura do Catálogo
 *
 * Mantém o catálogo de produtos e os mecanismos de sincronização.
 * Implementa o controle de acesso usando dois mutexes:
 * - mutex: protege o contador de leitores
 * - write_mutex: garante exclusão mútua para escritores
 */
typedef struct
{
    Product products[MAX_PRODUCTS]; // Array de produtos
    int num_readers;                // Contador de leitores ativos
    pthread_mutex_t mutex;          // Protege num_readers
    pthread_mutex_t write_mutex;    // Exclusão mútua para escritores
} Catalog;

// Instância global do catálogo
Catalog catalog = {
    .num_readers = 0};

/**
 * Inicializa o Catálogo
 *
 * Configura o estado inicial do sistema:
 * 1. Inicializa os mutexes para sincronização
 * 2. Popula o catálogo com produtos simulados
 */
void init_catalog()
{
    // Inicializa mutexes
    pthread_mutex_init(&catalog.mutex, NULL);
    pthread_mutex_init(&catalog.write_mutex, NULL);

    // Cria produtos com dados simulados
    for (int i = 0; i < MAX_PRODUCTS; i++)
    {
        catalog.products[i].id = i + 1;
        catalog.products[i].price = 10.0 + (rand() % 1000); // Preço entre R$10 e R$1010
        catalog.products[i].stock = rand() % 50;            // Estoque entre 0 e 49
    }
}

/**
 * Thread Leitora - Cliente
 *
 * Simula o comportamento de um cliente consultando produtos.
 * Implementa o protocolo de leitores com as seguintes etapas:
 * 1. Solicita acesso à leitura
 * 2. Realiza a consulta do produto
 * 3. Libera o acesso
 *
 * O primeiro leitor bloqueia escritores e o último libera.
 *
 * @param arg Ponteiro para o ID do cliente
 * @return NULL
 */
void *reader(void *arg)
{
    int id = *(int *)arg;

    for (int i = 0; i < NUM_READS; i++)
    {
        // Protocolo de entrada - Início da leitura
        pthread_mutex_lock(&catalog.mutex);
        catalog.num_readers++;
        if (catalog.num_readers == 1)
        {
            pthread_mutex_lock(&catalog.write_mutex); // Primeiro leitor bloqueia escritores
        }
        pthread_mutex_unlock(&catalog.mutex);

        // Seção crítica - Consulta do produto
        int product_id = rand() % MAX_PRODUCTS;
        Product product = catalog.products[product_id];
        printf("Cliente %d consultando produto %d: Preço = R$%.2f, Estoque = %d\n",
               id, product.id, product.price, product.stock);

        usleep(rand() % 500000); // Simula tempo de consulta (0-500ms)

        // Protocolo de saída - Fim da leitura
        pthread_mutex_lock(&catalog.mutex);
        catalog.num_readers--;
        if (catalog.num_readers == 0)
        {
            pthread_mutex_unlock(&catalog.write_mutex); // Último leitor libera escritores
        }
        pthread_mutex_unlock(&catalog.mutex);

        usleep(rand() % 1000000); // Intervalo entre consultas (0-1s)
    }

    printf("Cliente %d finalizou suas consultas\n", id);
    return NULL;
}

/**
 * Thread Escritora - Funcionário
 *
 * Simula o comportamento de um funcionário atualizando produtos.
 * Implementa exclusão mútua para garantir atualizações seguras:
 * 1. Obtém acesso exclusivo ao catálogo
 * 2. Atualiza informações do produto
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
        // Protocolo de entrada - Início da escrita
        pthread_mutex_lock(&catalog.write_mutex);

        // Seção crítica - Atualização do produto
        int product_id = rand() % MAX_PRODUCTS;
        float price_change = (rand() % 20) - 10; // Variação de -10% a +10%
        int stock_change = (rand() % 10) - 3;    // Variação de -3 a +6

        Product *product = &catalog.products[product_id];
        product->price *= (1 + price_change / 100.0);
        product->stock = product->stock + stock_change;
        if (product->stock < 0)
            product->stock = 0;

        printf("Funcionário %d atualizando produto %d: Novo preço = R$%.2f, Novo estoque = %d\n",
               id, product->id, product->price, product->stock);

        usleep(rand() % 1000000); // Simula tempo de atualização (0-1s)

        // Protocolo de saída - Fim da escrita
        pthread_mutex_unlock(&catalog.write_mutex);

        usleep(rand() % 2000000); // Intervalo entre atualizações (0-2s)
    }

    printf("Funcionário %d finalizou suas atualizações\n", id);
    return NULL;
}

/**
 * Função Principal
 *
 * Coordena a execução do sistema através das seguintes etapas:
 * 1. Inicializa o catálogo e mecanismos de sincronização
 * 2. Cria threads de clientes e funcionários
 * 3. Aguarda conclusão das operações
 * 4. Realiza limpeza dos recursos
 *
 * @return 0 em caso de sucesso
 */
int main()
{
    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    int reader_ids[NUM_READERS];
    int writer_ids[NUM_WRITERS];

    // Inicializa sistema
    init_catalog();

    // Cria threads de clientes (leitores)
    for (int i = 0; i < NUM_READERS; i++)
    {
        reader_ids[i] = i + 1;
        if (pthread_create(&readers[i], NULL, reader, &reader_ids[i]) != 0)
        {
            fprintf(stderr, "Erro ao criar thread de cliente %d\n", i);
            return 1;
        }
    }

    // Cria threads de funcionários (escritores)
    for (int i = 0; i < NUM_WRITERS; i++)
    {
        writer_ids[i] = i + 1;
        if (pthread_create(&writers[i], NULL, writer, &writer_ids[i]) != 0)
        {
            fprintf(stderr, "Erro ao criar thread de funcionário %d\n", i);
            return 1;
        }
    }

    // Aguarda término das threads
    for (int i = 0; i < NUM_READERS; i++)
    {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < NUM_WRITERS; i++)
    {
        pthread_join(writers[i], NULL);
    }

    // Libera recursos
    pthread_mutex_destroy(&catalog.mutex);
    pthread_mutex_destroy(&catalog.write_mutex);

    printf("Sistema finalizado com sucesso\n");
    return 0;
}
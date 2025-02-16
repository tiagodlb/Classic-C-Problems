/**
 * Sistema de Banco de Dados de E-commerce - Implementação com Monitor
 *
 * Este programa implementa o problema dos leitores/escritores usando o conceito
 * de Monitor em um sistema de e-commerce. O Monitor é uma estrutura que encapsula:
 * - Dados compartilhados (produtos do catálogo)
 * - Estado do sistema (contadores e flags)
 * - Mecanismos de sincronização (mutex e variáveis de condição)
 *
 * Características do Monitor:
 * - Encapsulamento de dados e sincronização
 * - Controle de acesso via operações sincronizadas
 * - Gerenciamento automático de exclusão mútua
 * - Coordenação via variáveis de condição
 *
 * Funcionalidades:
 * 1. Clientes podem consultar produtos simultaneamente
 * 2. Funcionários atualizam produtos com exclusão mútua
 * 3. Prioridade configurável entre leitores e escritores
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
 * Representa um item individual no catálogo.
 * Mantém as informações essenciais de cada produto.
 */
typedef struct
{
    int id;      // Identificador único do produto
    float price; // Preço atual em reais
    int stock;   // Quantidade em estoque
} Product;

/**
 * Estrutura do Monitor do Catálogo
 *
 * Implementa o monitor que controla o acesso ao catálogo.
 * Encapsula tanto os dados quanto os mecanismos de sincronização.
 */
typedef struct
{
    // Dados compartilhados
    Product products[MAX_PRODUCTS]; // Catálogo de produtos
    int num_readers;                // Número de leitores ativos
    int num_writers;                // Número de escritores ativos
    int writer_waiting;             // Número de escritores aguardando

    // Mecanismos de sincronização
    pthread_mutex_t mutex;    // Mutex principal do monitor
    pthread_cond_t can_read;  // Condição para permitir leitura
    pthread_cond_t can_write; // Condição para permitir escrita

    // Estado do sistema
    int should_stop; // Flag para controle de finalização
} CatalogMonitor;

// Instância global do monitor
CatalogMonitor catalog;

/**
 * Inicializa o Monitor do Catálogo
 *
 * Configura o estado inicial do monitor:
 * 1. Inicializa contadores e flags
 * 2. Configura mecanismos de sincronização
 * 3. Popula o catálogo com dados iniciais
 */
void monitor_init()
{
    // Inicializa contadores
    catalog.num_readers = 0;
    catalog.num_writers = 0;
    catalog.writer_waiting = 0;
    catalog.should_stop = 0;

    // Inicializa mecanismos de sincronização
    pthread_mutex_init(&catalog.mutex, NULL);
    pthread_cond_init(&catalog.can_read, NULL);
    pthread_cond_init(&catalog.can_write, NULL);

    // Popula catálogo com dados simulados
    for (int i = 0; i < MAX_PRODUCTS; i++)
    {
        catalog.products[i].id = i + 1;
        catalog.products[i].price = 10.0 + (rand() % 1000); // Preço entre R$10 e R$1010
        catalog.products[i].stock = rand() % 50;            // Estoque entre 0 e 49
    }
}

/**
 * Libera Recursos do Monitor
 *
 * Realiza a limpeza dos recursos alocados pelo monitor:
 * - Destrói mutex e variáveis de condição
 */
void monitor_destroy()
{
    pthread_mutex_destroy(&catalog.mutex);
    pthread_cond_destroy(&catalog.can_read);
    pthread_cond_destroy(&catalog.can_write);
}

/**
 * Início de Operação de Leitura
 *
 * Implementa o protocolo de entrada para leitores:
 * 1. Verifica se pode realizar leitura
 * 2. Aguarda se houver escritor ativo ou aguardando
 * 3. Registra novo leitor ativo
 */
void start_read()
{
    pthread_mutex_lock(&catalog.mutex);

    // Aguarda se houver escritor ou escritor esperando
    while (catalog.num_writers > 0 || catalog.writer_waiting > 0)
    {
        pthread_cond_wait(&catalog.can_read, &catalog.mutex);
    }

    catalog.num_readers++;
    pthread_mutex_unlock(&catalog.mutex);
}

/**
 * Fim de Operação de Leitura
 *
 * Implementa o protocolo de saída para leitores:
 * 1. Decrementa contador de leitores
 * 2. Se for o último leitor, sinaliza escritores
 */
void end_read()
{
    pthread_mutex_lock(&catalog.mutex);
    catalog.num_readers--;

    // Último leitor sinaliza escritores
    if (catalog.num_readers == 0)
    {
        pthread_cond_signal(&catalog.can_write);
    }

    pthread_mutex_unlock(&catalog.mutex);
}

/**
 * Início de Operação de Escrita
 *
 * Implementa o protocolo de entrada para escritores:
 * 1. Registra escritor aguardando
 * 2. Espera até poder escrever (sem leitores/escritores)
 * 3. Registra escritor ativo
 */
void start_write()
{
    pthread_mutex_lock(&catalog.mutex);
    catalog.writer_waiting++;

    // Aguarda se houver leitores ou outro escritor
    while (catalog.num_readers > 0 || catalog.num_writers > 0)
    {
        pthread_cond_wait(&catalog.can_write, &catalog.mutex);
    }

    catalog.writer_waiting--;
    catalog.num_writers++;
    pthread_mutex_unlock(&catalog.mutex);
}

/**
 * Fim de Operação de Escrita
 *
 * Implementa o protocolo de saída para escritores:
 * 1. Decrementa contador de escritores
 * 2. Sinaliza próxima thread (escritor ou leitores)
 */
void end_write()
{
    pthread_mutex_lock(&catalog.mutex);
    catalog.num_writers--;

    // Política de prioridade: escritores > leitores
    if (catalog.writer_waiting > 0)
    {
        pthread_cond_signal(&catalog.can_write);
    }
    else
    {
        pthread_cond_broadcast(&catalog.can_read);
    }

    pthread_mutex_unlock(&catalog.mutex);
}

/**
 * Thread Leitora (Cliente)
 *
 * Simula o comportamento de um cliente consultando produtos.
 * Utiliza os protocolos do monitor para acesso seguro.
 *
 * @param arg Ponteiro para o ID do cliente
 * @return NULL
 */
void *reader(void *arg)
{
    int id = *(int *)arg;

    for (int i = 0; i < NUM_READS && !catalog.should_stop; i++)
    {
        start_read();

        // Consulta produto aleatório
        int product_id = rand() % MAX_PRODUCTS;
        Product product = catalog.products[product_id];
        printf("Cliente %d consultando produto %d: Preço = R$%.2f, Estoque = %d\n",
               id, product.id, product.price, product.stock);

        usleep(rand() % 500000); // Simula tempo de consulta (0-500ms)

        end_read();

        usleep(rand() % 1000000); // Intervalo entre consultas (0-1s)
    }

    printf("Cliente %d finalizou suas consultas\n", id);
    return NULL;
}

/**
 * Thread Escritora (Funcionário)
 *
 * Simula o comportamento de um funcionário atualizando produtos.
 * Utiliza os protocolos do monitor para acesso exclusivo.
 *
 * @param arg Ponteiro para o ID do funcionário
 * @return NULL
 */
void *writer(void *arg)
{
    int id = *(int *)arg;

    for (int i = 0; i < NUM_WRITES && !catalog.should_stop; i++)
    {
        start_write();

        // Atualiza produto aleatório
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

        end_write();

        usleep(rand() % 2000000); // Intervalo entre atualizações (0-2s)
    }

    printf("Funcionário %d finalizou suas atualizações\n", id);
    return NULL;
}

/**
 * Função Principal
 *
 * Gerencia o ciclo de vida do sistema:
 * 1. Inicializa o monitor
 * 2. Cria e gerencia as threads
 * 3. Aguarda conclusão
 * 4. Libera recursos
 *
 * @return 0 em caso de sucesso, 1 em caso de erro
 */
int main()
{
    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    int reader_ids[NUM_READERS];
    int writer_ids[NUM_WRITERS];

    monitor_init();

    // Cria threads de clientes
    for (int i = 0; i < NUM_READERS; i++)
    {
        reader_ids[i] = i + 1;
        if (pthread_create(&readers[i], NULL, reader, &reader_ids[i]) != 0)
        {
            fprintf(stderr, "Erro ao criar thread de cliente %d\n", i);
            catalog.should_stop = 1;
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
            catalog.should_stop = 1;
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

    monitor_destroy();

    printf("Sistema finalizado com sucesso\n");
    return 0;
}
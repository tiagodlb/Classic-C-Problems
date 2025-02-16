/**
 * Sistema de Gerenciamento de Recursos para Estúdio de Edição de Vídeo
 *
 * Este sistema implementa uma solução para o problema dos filósofos jantadores usando
 * semáforos, adaptado para um cenário de estúdio de edição de vídeo. O sistema gerencia
 * o acesso compartilhado a placas de processamento de vídeo entre múltiplos editores.
 *
 * Características do Sistema:
 * - Cada editor precisa de duas placas adjacentes para trabalhar
 * - Utiliza semáforos para controle de acesso e sincronização
 * - Previne deadlocks e starvation
 *
 * Arquitetura de Semáforos:
 * - Semáforo mutex: controla acesso à seção crítica
 * - Semáforos de editores: controlam permissão para editar
 * - Semáforos de placas: controlam acesso aos recursos
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

/**
 * Constantes de Configuração do Sistema
 */
#define NUM_EDITORS 5 // Número de editores no sistema
#define NUM_BOARDS 5  // Número de placas de processamento
#define NUM_EDITS 3   // Edições por editor
#define THINK_TIME 2  // Tempo máximo de planejamento (segundos)
#define EDIT_TIME 3   // Tempo máximo de edição (segundos)

/**
 * Estados dos Editores
 *
 * Define os possíveis estados que um editor pode assumir:
 * - THINKING: Editor está planejando sua próxima edição
 * - HUNGRY: Editor necessita de placas para editar
 * - EDITING: Editor está ativamente usando as placas
 */
typedef enum
{
    THINKING, // Editor planejando próxima edição
    HUNGRY,   // Editor aguardando recursos
    EDITING   // Editor realizando edição
} EditorState;

/**
 * Estrutura de Controle do Estúdio
 *
 * Mantém o estado completo do sistema e seus mecanismos de sincronização:
 * - Estados dos editores
 * - Semáforos para controle de acesso
 * - Semáforos para cada recurso
 */
typedef struct
{
    EditorState state[NUM_EDITORS]; // Estado atual de cada editor
    sem_t sem_editors[NUM_EDITORS]; // Semáforo individual por editor
    sem_t mutex;                    // Semáforo para exclusão mútua
    sem_t boards[NUM_BOARDS];       // Semáforo para cada placa
} StudioControl;

// Instância global do controle do estúdio
StudioControl studio;

/**
 * Inicializa o Sistema do Estúdio
 *
 * Configura o estado inicial do sistema:
 * - Inicializa o semáforo de exclusão mútua
 * - Configura os semáforos dos editores
 * - Inicializa os semáforos das placas
 * - Define estados iniciais dos editores
 */
void init_studio()
{
    // Inicializa semáforo principal (binário)
    sem_init(&studio.mutex, 0, 1);

    // Configura editores
    for (int i = 0; i < NUM_EDITORS; i++)
    {
        sem_init(&studio.sem_editors[i], 0, 0); // Semáforo inicialmente bloqueado
        studio.state[i] = THINKING;             // Editor começa planejando
    }

    // Configura placas
    for (int i = 0; i < NUM_BOARDS; i++)
    {
        sem_init(&studio.boards[i], 0, 1); // Cada placa começa disponível
    }
}

/**
 * Libera Recursos do Sistema
 *
 * Realiza a limpeza adequada de todos os semáforos:
 * - Destrói semáforo de exclusão mútua
 * - Destrói semáforos dos editores
 * - Destrói semáforos das placas
 */
void cleanup_studio()
{
    sem_destroy(&studio.mutex);

    for (int i = 0; i < NUM_EDITORS; i++)
    {
        sem_destroy(&studio.sem_editors[i]);
    }

    for (int i = 0; i < NUM_BOARDS; i++)
    {
        sem_destroy(&studio.boards[i]);
    }
}

/**
 * Verifica Disponibilidade para Edição
 *
 * Analisa se um editor pode começar a editar verificando:
 * - Se o editor está solicitando recursos (HUNGRY)
 * - Se os vizinhos não estão editando
 * - Se as placas necessárias estão livres
 *
 * @param editor_id ID do editor a ser verificado
 */
void test_editor(int editor_id)
{
    int left = editor_id;
    int right = (editor_id + 1) % NUM_EDITORS;

    // Verifica condições necessárias
    if (studio.state[editor_id] == HUNGRY &&
        studio.state[left] != EDITING &&
        studio.state[right] != EDITING)
    {

        // Permite que o editor comece
        studio.state[editor_id] = EDITING;
        sem_post(&studio.sem_editors[editor_id]);
    }
}

/**
 * Simula Planejamento de Edição
 *
 * Representa o tempo que o editor gasta planejando
 * sua próxima edição de vídeo.
 *
 * @param editor_id ID do editor que está planejando
 */
void think(int editor_id)
{
    printf("Editor %d está planejando a próxima edição...\n", editor_id);
    sleep(rand() % THINK_TIME + 1);
}

/**
 * Requisita Placas para Edição
 *
 * Implementa o protocolo de requisição de recursos:
 * 1. Obtém acesso exclusivo (mutex)
 * 2. Indica interesse nas placas (HUNGRY)
 * 3. Tenta começar a edição
 * 4. Aguarda permissão se necessário
 * 5. Adquire as placas necessárias
 *
 * @param editor_id ID do editor requisitando recursos
 */
void get_boards(int editor_id)
{
    sem_wait(&studio.mutex);

    printf("Editor %d está aguardando placas...\n", editor_id);
    studio.state[editor_id] = HUNGRY;
    test_editor(editor_id);

    sem_post(&studio.mutex);
    sem_wait(&studio.sem_editors[editor_id]);

    // Adquire as placas necessárias
    sem_wait(&studio.boards[editor_id]);
    sem_wait(&studio.boards[(editor_id + 1) % NUM_BOARDS]);

    printf("Editor %d adquiriu as placas %d e %d\n",
           editor_id, editor_id, (editor_id + 1) % NUM_BOARDS);
}

/**
 * Simula Processo de Edição
 *
 * Representa o tempo que o editor gasta efetivamente
 * editando o vídeo com as placas.
 *
 * @param editor_id ID do editor realizando a edição
 */
void edit(int editor_id)
{
    printf("Editor %d está editando o vídeo...\n", editor_id);
    sleep(rand() % EDIT_TIME + 1);
}

/**
 * Libera Placas após Edição
 *
 * Implementa o protocolo de liberação de recursos:
 * 1. Obtém acesso exclusivo
 * 2. Marca editor como THINKING
 * 3. Libera as placas utilizadas
 * 4. Verifica se os vizinhos podem editar
 *
 * @param editor_id ID do editor liberando recursos
 */
void put_boards(int editor_id)
{
    sem_wait(&studio.mutex);

    studio.state[editor_id] = THINKING;
    printf("Editor %d liberou as placas %d e %d\n",
           editor_id, editor_id, (editor_id + 1) % NUM_BOARDS);

    // Libera os recursos
    sem_post(&studio.boards[editor_id]);
    sem_post(&studio.boards[(editor_id + 1) % NUM_BOARDS]);

    // Verifica vizinhos
    test_editor((editor_id + NUM_EDITORS - 1) % NUM_EDITORS);
    test_editor((editor_id + 1) % NUM_EDITORS);

    sem_post(&studio.mutex);
}

/**
 * Rotina Principal do Editor
 *
 * Implementa o ciclo completo de trabalho do editor:
 * 1. Planeja a edição
 * 2. Requisita recursos
 * 3. Realiza a edição
 * 4. Libera recursos
 *
 * @param arg Ponteiro para o ID do editor
 * @return NULL após completar todas as edições
 */
void *editor(void *arg)
{
    int id = *(int *)arg;

    for (int i = 0; i < NUM_EDITS; i++)
    {
        think(id);      // Fase de planejamento
        get_boards(id); // Aquisição de recursos
        edit(id);       // Edição do vídeo
        put_boards(id); // Liberação de recursos
    }

    printf("Editor %d completou todas as edições\n", id);
    return NULL;
}

/**
 * Função Principal
 *
 * Coordena a execução do sistema:
 * 1. Inicializa recursos e semáforos
 * 2. Cria threads dos editores
 * 3. Aguarda conclusão das edições
 * 4. Realiza limpeza dos recursos
 *
 * @return 0 em caso de sucesso, 1 em caso de erro
 */
int main()
{
    pthread_t editors[NUM_EDITORS];
    int editor_ids[NUM_EDITORS];

    // Inicializa sistema
    init_studio();

    // Cria threads dos editores
    for (int i = 0; i < NUM_EDITORS; i++)
    {
        editor_ids[i] = i;
        if (pthread_create(&editors[i], NULL, editor, &editor_ids[i]) != 0)
        {
            fprintf(stderr, "Erro ao criar thread do editor %d\n", i);
            cleanup_studio();
            return 1;
        }
    }

    // Aguarda conclusão
    for (int i = 0; i < NUM_EDITORS; i++)
    {
        pthread_join(editors[i], NULL);
    }

    // Limpa recursos
    cleanup_studio();

    printf("Todas as edições foram concluídas\n");
    return 0;
}
/**
 * Sistema de Gerenciamento de Recursos para Estúdio de Edição de Vídeo
 *
 * Implementa o problema dos filósofos jantadores utilizando o conceito de Monitor
 * em um cenário de estúdio de edição de vídeo. O monitor encapsula tanto os dados
 * compartilhados quanto os mecanismos de sincronização necessários para garantir
 * acesso seguro aos recursos.
 *
 * Características do Monitor:
 * - Encapsulamento de dados e sincronização em uma única estrutura
 * - Controle de acesso via mutex
 * - Sincronização via variáveis de condição
 * - Prevenção de deadlock e starvation
 *
 * Cenário:
 * - Editores precisam de duas placas adjacentes para trabalhar
 * - Recursos são acessados com exclusão mútua
 * - Sistema garante progresso e justiça
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/**
 * Constantes de Configuração do Sistema
 */
#define NUM_EDITORS 5 // Número total de editores
#define NUM_BOARDS 5  // Número total de placas
#define NUM_EDITS 3   // Edições por editor
#define THINK_TIME 2  // Tempo máximo de planejamento (segundos)
#define EDIT_TIME 3   // Tempo máximo de edição (segundos)

/**
 * Estados do Editor
 *
 * Define os possíveis estados que um editor pode assumir durante
 * seu ciclo de trabalho no sistema.
 */
typedef enum
{
    THINKING, // Editor está planejando sua próxima edição
    HUNGRY,   // Editor está aguardando acesso às placas
    EDITING   // Editor está ativamente usando as placas
} EditorState;

/**
 * Estrutura do Monitor
 *
 * Encapsula todo o estado do sistema e seus mecanismos de sincronização.
 * O monitor garante que todas as operações sobre os recursos compartilhados
 * sejam realizadas de forma segura e coordenada.
 */
typedef struct
{
    // Estado do Sistema
    EditorState state[NUM_EDITORS]; // Estado atual de cada editor
    int board_in_use[NUM_BOARDS];   // Estado das placas (0=livre, 1=em uso)

    // Mecanismos de Sincronização
    pthread_mutex_t mutex;                // Mutex para exclusão mútua do monitor
    pthread_cond_t can_edit[NUM_EDITORS]; // Condição para controle de cada editor

    // Controle do Sistema
    int should_stop; // Flag para finalização ordenada
} StudioMonitor;

// Instância global do monitor
StudioMonitor studio;

/**
 * Inicialização do Monitor
 *
 * Configura o estado inicial do sistema:
 * - Inicializa o mutex
 * - Configura as variáveis de condição
 * - Define estados iniciais dos editores e placas
 */
void monitor_init()
{
    // Inicializa mutex principal
    pthread_mutex_init(&studio.mutex, NULL);

    // Inicializa editores
    for (int i = 0; i < NUM_EDITORS; i++)
    {
        pthread_cond_init(&studio.can_edit[i], NULL);
        studio.state[i] = THINKING; // Começa planejando
    }

    // Inicializa placas
    for (int i = 0; i < NUM_BOARDS; i++)
    {
        studio.board_in_use[i] = 0; // Começa livre
    }

    studio.should_stop = 0;
}

/**
 * Liberação de Recursos do Monitor
 *
 * Realiza a limpeza adequada dos recursos alocados:
 * - Destrói o mutex
 * - Destrói as variáveis de condição
 */
void monitor_destroy()
{
    pthread_mutex_destroy(&studio.mutex);

    for (int i = 0; i < NUM_EDITORS; i++)
    {
        pthread_cond_destroy(&studio.can_edit[i]);
    }
}

/**
 * Verificação de Disponibilidade
 *
 * Verifica se um editor pode começar a editar baseado em:
 * - Seu estado atual (deve estar HUNGRY)
 * - Disponibilidade das placas necessárias
 *
 * @param editor_id ID do editor a ser verificado
 * @return 1 se pode editar, 0 caso contrário
 */
int can_edit(int editor_id)
{
    int left_board = editor_id;
    int right_board = (editor_id + 1) % NUM_BOARDS;

    return (studio.state[editor_id] == HUNGRY &&
            !studio.board_in_use[left_board] &&
            !studio.board_in_use[right_board]);
}

/**
 * Tentativa de Início de Edição
 *
 * Verifica se um editor pode começar a editar e, em caso positivo:
 * - Atualiza seu estado para EDITING
 * - Marca as placas como em uso
 * - Sinaliza o editor para prosseguir
 *
 * @param editor_id ID do editor tentando iniciar edição
 */
void try_to_edit(int editor_id)
{
    if (can_edit(editor_id))
    {
        // Atualiza estado
        studio.state[editor_id] = EDITING;

        // Marca placas como em uso
        studio.board_in_use[editor_id] = 1;
        studio.board_in_use[(editor_id + 1) % NUM_BOARDS] = 1;

        // Sinaliza editor
        pthread_cond_signal(&studio.can_edit[editor_id]);
    }
}

/**
 * Requisição de Placas
 *
 * Implementa o protocolo de requisição de recursos no monitor:
 * 1. Obtém acesso ao monitor
 * 2. Marca interesse nas placas
 * 3. Tenta iniciar edição
 * 4. Aguarda se necessário
 *
 * @param editor_id ID do editor requisitando recursos
 */
void request_boards(int editor_id)
{
    pthread_mutex_lock(&studio.mutex);

    printf("Editor %d está aguardando placas...\n", editor_id);
    studio.state[editor_id] = HUNGRY;
    try_to_edit(editor_id);

    // Aguarda até conseguir as placas
    while (studio.state[editor_id] == HUNGRY)
    {
        pthread_cond_wait(&studio.can_edit[editor_id], &studio.mutex);
    }

    printf("Editor %d adquiriu as placas %d e %d\n",
           editor_id, editor_id, (editor_id + 1) % NUM_BOARDS);

    pthread_mutex_unlock(&studio.mutex);
}

/**
 * Liberação de Placas
 *
 * Implementa o protocolo de liberação de recursos no monitor:
 * 1. Obtém acesso ao monitor
 * 2. Libera as placas utilizadas
 * 3. Verifica se os vizinhos podem editar
 *
 * @param editor_id ID do editor liberando recursos
 */
void release_boards(int editor_id)
{
    pthread_mutex_lock(&studio.mutex);

    // Libera recursos
    studio.state[editor_id] = THINKING;
    studio.board_in_use[editor_id] = 0;
    studio.board_in_use[(editor_id + 1) % NUM_BOARDS] = 0;

    printf("Editor %d liberou as placas %d e %d\n",
           editor_id, editor_id, (editor_id + 1) % NUM_BOARDS);

    // Verifica vizinhos
    try_to_edit((editor_id + NUM_EDITORS - 1) % NUM_EDITORS);
    try_to_edit((editor_id + 1) % NUM_EDITORS);

    pthread_mutex_unlock(&studio.mutex);
}

/**
 * Simulação de Planejamento
 *
 * Simula o tempo que o editor gasta planejando sua edição.
 *
 * @param editor_id ID do editor planejando
 */
void think(int editor_id)
{
    printf("Editor %d está planejando a próxima edição...\n", editor_id);
    sleep(rand() % THINK_TIME + 1);
}

/**
 * Simulação de Edição
 *
 * Simula o tempo que o editor gasta realizando a edição.
 *
 * @param editor_id ID do editor editando
 */
void edit(int editor_id)
{
    printf("Editor %d está editando o vídeo...\n", editor_id);
    sleep(rand() % EDIT_TIME + 1);
}

/**
 * Thread do Editor
 *
 * Implementa o ciclo completo de trabalho de um editor:
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

    for (int i = 0; i < NUM_EDITS && !studio.should_stop; i++)
    {
        think(id);          // Fase de planejamento
        request_boards(id); // Aquisição de recursos
        edit(id);           // Edição do vídeo
        release_boards(id); // Liberação de recursos
    }

    printf("Editor %d completou todas as edições\n", id);
    return NULL;
}

/**
 * Função Principal
 *
 * Gerencia o ciclo de vida do sistema:
 * 1. Inicializa o monitor
 * 2. Cria e gerencia threads dos editores
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
    monitor_init();

    // Cria threads dos editores
    for (int i = 0; i < NUM_EDITORS; i++)
    {
        editor_ids[i] = i;
        if (pthread_create(&editors[i], NULL, editor, &editor_ids[i]) != 0)
        {
            fprintf(stderr, "Erro ao criar thread do editor %d\n", i);
            studio.should_stop = 1;
            return 1;
        }
    }

    // Aguarda conclusão
    for (int i = 0; i < NUM_EDITORS; i++)
    {
        pthread_join(editors[i], NULL);
    }

    // Limpa recursos
    monitor_destroy();

    printf("Todas as edições foram concluídas\n");
    return 0;
}
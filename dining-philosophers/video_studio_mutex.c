/**
 * Sistema de Gerenciamento de Recursos para Estúdio de Edição de Vídeo
 *
 * Este sistema implementa uma solução para o problema clássico dos filósofos jantadores,
 * adaptado para um cenário de estúdio de edição de vídeo. O problema envolve múltiplos
 * editores que precisam compartilhar placas de processamento de vídeo.
 *
 * Cenário:
 * - Editores precisam de duas placas adjacentes para realizar uma edição
 * - Placas são recursos compartilhados que não podem ser usados simultaneamente
 * - Cada editor passa por ciclos de planejamento e edição
 *
 * Desafios Resolvidos:
 * 1. Deadlock: Evita-se que editores fiquem eternamente esperando recursos
 * 2. Starvation: Garante-se que todos os editores conseguem acessar os recursos
 * 3. Race Conditions: Protege-se o acesso aos recursos compartilhados
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/**
 * Constantes de Configuração do Sistema
 */
#define NUM_EDITORS 5 // Número total de editores no sistema
#define NUM_BOARDS 5  // Número total de placas de processamento
#define NUM_EDITS 3   // Número de edições que cada editor deve realizar
#define THINK_TIME 2  // Tempo máximo de planejamento (segundos)
#define EDIT_TIME 3   // Tempo máximo de edição (segundos)

/**
 * Estados Possíveis de um Editor
 *
 * Define os diferentes estados que um editor pode assumir durante
 * seu ciclo de trabalho.
 */
typedef enum
{
    THINKING, // Editor está planejando sua próxima edição
    HUNGRY,   // Editor está aguardando placas disponíveis
    EDITING   // Editor está ativamente editando um vídeo
} EditorState;

/**
 * Estrutura de Controle do Estúdio
 *
 * Mantém o estado completo do sistema, incluindo:
 * - Estado de cada editor
 * - Estado de cada placa
 * - Mecanismos de sincronização
 */
typedef struct
{
    EditorState state[NUM_EDITORS];   // Estado atual de cada editor
    int has_board[NUM_BOARDS];        // Indica se cada placa está em uso (0=livre, 1=em uso)
    pthread_mutex_t mutex;            // Mutex para proteção da seção crítica
    pthread_cond_t cond[NUM_EDITORS]; // Variável de condição para cada editor
} StudioControl;

// Instância global do controle do estúdio
StudioControl studio;

/**
 * Inicializa o Sistema do Estúdio
 *
 * Configura o estado inicial do sistema:
 * - Inicializa o mutex global
 * - Configura as variáveis de condição
 * - Define estados iniciais dos editores e placas
 */
void init_studio()
{
    pthread_mutex_init(&studio.mutex, NULL);

    // Inicializa editores
    for (int i = 0; i < NUM_EDITORS; i++)
    {
        studio.state[i] = THINKING; // Todos começam pensando
        pthread_cond_init(&studio.cond[i], NULL);
    }

    // Inicializa placas
    for (int i = 0; i < NUM_BOARDS; i++)
    {
        studio.has_board[i] = 0; // Todas começam livres
    }
}

/**
 * Libera Recursos do Sistema
 *
 * Realiza a limpeza adequada de todos os recursos alocados:
 * - Destrói o mutex global
 * - Destrói as variáveis de condição
 */
void cleanup_studio()
{
    pthread_mutex_destroy(&studio.mutex);
    for (int i = 0; i < NUM_EDITORS; i++)
    {
        pthread_cond_destroy(&studio.cond[i]);
    }
}

/**
 * Verifica se um Editor Pode Começar a Editar
 *
 * Determina se um editor pode iniciar sua edição baseado em:
 * - Seu estado atual (deve estar HUNGRY)
 * - Disponibilidade das placas necessárias (esquerda e direita)
 *
 * @param editor_id ID do editor a ser verificado
 * @return 1 se pode editar, 0 caso contrário
 */
int can_edit(int editor_id)
{
    int left = editor_id;
    int right = (editor_id + 1) % NUM_BOARDS;

    return (studio.state[editor_id] == HUNGRY &&
            !studio.has_board[left] &&
            !studio.has_board[right]);
}

/**
 * Tenta Iniciar uma Edição
 *
 * Verifica se um editor pode começar a editar e, em caso positivo:
 * - Atualiza seu estado para EDITING
 * - Marca as placas como em uso
 * - Sinaliza o editor que ele pode prosseguir
 *
 * @param editor_id ID do editor a tentar iniciar edição
 */
void test_editor(int editor_id)
{
    if (can_edit(editor_id))
    {
        studio.state[editor_id] = EDITING;
        studio.has_board[editor_id] = 1;
        studio.has_board[(editor_id + 1) % NUM_BOARDS] = 1;
        pthread_cond_signal(&studio.cond[editor_id]);
    }
}

/**
 * Simulação do Tempo de Planejamento
 *
 * Simula o editor planejando sua próxima edição.
 *
 * @param editor_id ID do editor que está planejando
 */
void think(int editor_id)
{
    printf("Editor %d está planejando a próxima edição...\n", editor_id);
    usleep((rand() % THINK_TIME) * 1000000);
}

/**
 * Tenta Adquirir Placas para Edição
 *
 * Implementa o protocolo de aquisição de recursos:
 * 1. Indica que está interessado (HUNGRY)
 * 2. Tenta começar a edição
 * 3. Aguarda se necessário
 *
 * @param editor_id ID do editor requisitando placas
 */
void take_boards(int editor_id)
{
    pthread_mutex_lock(&studio.mutex);

    printf("Editor %d está aguardando placas...\n", editor_id);
    studio.state[editor_id] = HUNGRY;
    test_editor(editor_id);

    // Aguarda até conseguir as placas
    while (studio.state[editor_id] == HUNGRY)
    {
        pthread_cond_wait(&studio.cond[editor_id], &studio.mutex);
    }

    printf("Editor %d adquiriu as placas %d e %d\n",
           editor_id, editor_id, (editor_id + 1) % NUM_BOARDS);

    pthread_mutex_unlock(&studio.mutex);
}

/**
 * Simula o Processo de Edição
 *
 * Representa o tempo que o editor passa efetivamente editando o vídeo.
 *
 * @param editor_id ID do editor realizando a edição
 */
void edit(int editor_id)
{
    printf("Editor %d está editando o vídeo...\n", editor_id);
    usleep((rand() % EDIT_TIME) * 1000000);
}

/**
 * Libera as Placas Após a Edição
 *
 * Implementa o protocolo de liberação de recursos:
 * 1. Marca o editor como THINKING
 * 2. Libera as placas
 * 3. Verifica se os vizinhos podem começar
 *
 * @param editor_id ID do editor liberando as placas
 */
void put_boards(int editor_id)
{
    pthread_mutex_lock(&studio.mutex);

    studio.state[editor_id] = THINKING;
    studio.has_board[editor_id] = 0;
    studio.has_board[(editor_id + 1) % NUM_BOARDS] = 0;

    printf("Editor %d liberou as placas %d e %d\n",
           editor_id, editor_id, (editor_id + 1) % NUM_BOARDS);

    // Verifica se os vizinhos podem começar
    test_editor((editor_id + NUM_EDITORS - 1) % NUM_EDITORS);
    test_editor((editor_id + 1) % NUM_EDITORS);

    pthread_mutex_unlock(&studio.mutex);
}

/**
 * Rotina Principal do Editor
 *
 * Implementa o ciclo completo de trabalho de um editor:
 * 1. Planeja a edição
 * 2. Adquire recursos
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
        think(id);       // Fase de planejamento
        take_boards(id); // Aquisição de recursos
        edit(id);        // Edição do vídeo
        put_boards(id);  // Liberação de recursos
    }

    printf("Editor %d completou todas as edições\n", id);
    return NULL;
}

/**
 * Função Principal
 *
 * Inicializa o sistema e gerencia o ciclo de vida dos editores:
 * 1. Configura o sistema
 * 2. Cria as threads dos editores
 * 3. Aguarda conclusão
 * 4. Libera recursos
 *
 * @return 0 em caso de sucesso, 1 em caso de erro
 */
int main()
{
    pthread_t editors[NUM_EDITORS];
    int editor_ids[NUM_EDITORS];

    srand(time(NULL));
    init_studio();

    printf("Iniciando sistema do estúdio com %d editores\n", NUM_EDITORS);

    // Cria as threads dos editores
    for (int i = 0; i < NUM_EDITORS; i++)
    {
        editor_ids[i] = i;
        if (pthread_create(&editors[i], NULL, editor, &editor_ids[i]) != 0)
        {
            fprintf(stderr, "Erro ao criar editor %d\n", i);
            cleanup_studio();
            return 1;
        }
    }

    // Aguarda conclusão de todas as threads
    for (int i = 0; i < NUM_EDITORS; i++)
    {
        pthread_join(editors[i], NULL);
    }

    cleanup_studio();
    printf("Sistema finalizado com sucesso\n");
    return 0;
}
# Problemas Clássicos de Sincronização em C

Este repositório contém implementações de três problemas clássicos de sincronização usando diferentes mecanismos em C:

1. **Produtor-Consumidor (Bounded Buffer)**
2. **Leitores-Escritores (Readers-Writers)**
3. **Filósofos Jantadores (Dining Philosophers)**

Cada problema é implementado usando três diferentes abordagens de sincronização:

- Mutex
- Semáforos
- Monitores

## Estrutura do Projeto

```
.
├── bound-buffer/
│   ├── compiled/
│   │   ├── mutex
│   │   ├── semaphore
│   │   └── monitor
│   ├── mutex.c
│   ├── semaphore.c
│   └── monitor.c
├── dining-philosophers/
│   ├── compiled/
│   │   ├── mutex
│   │   ├── semaphore
│   │   └── monitor
│   ├── mutex.c
│   ├── semaphore.c
│   └── monitor.c
└── readers-writers/
    ├── compiled/
    │   ├── mutex
    │   ├── semaphore
    │   └── monitor
    ├── mutex.c
    ├── semaphore.c
    └── monitor.c
```

## Requisitos

- GCC (GNU Compiler Collection)
- Sistema operacional Linux/Unix
- Biblioteca pthread
- Make (opcional, mas recomendado)

## Dependências

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install gcc make

# Fedora
sudo dnf install gcc make

# CentOS/RHEL
sudo yum install gcc make
```

## Compilação

Para compilar cada programa individualmente:

```bash
gcc -o programa programa.c -pthread
```

Exemplo para cada implementação:

```bash
# Bound Buffer - Mutex
gcc -o bound-buffer/compiled/mutex bound-buffer/mutex.c -pthread

# Readers-Writers - Semaphore
gcc -o readers-writers/compiled/semaphore readers-writers/semaphore.c -pthread

# Dining Philosophers - Monitor
gcc -o dining-philosophers/compiled/monitor dining-philosophers/monitor.c -pthread
```

## Execução

Para executar qualquer programa compilado:

```bash
./programa
```

Exemplo:

```bash
# Bound Buffer - Mutex
./bound-buffer/compiled/mutex

# Readers-Writers - Semaphore
./readers-writers/compiled/semaphore

# Dining Philosophers - Monitor
./dining-philosophers/compiled/monitor
```

## Implementações

### Bound Buffer (Produtor-Consumidor)

- **Mutex**: Implementação usando mutex e variáveis de condição
- **Semaphore**: Implementação usando semáforos POSIX
- **Monitor**: Implementação usando o conceito de monitores

### Readers-Writers (Leitores-Escritores)

- **Mutex**: Implementação priorizando leitores usando mutex
- **Semaphore**: Implementação com semáforos garantindo exclusão mútua para escritores
- **Monitor**: Implementação usando monitor com variáveis de condição

### Dining Philosophers (Filósofos Jantadores)

- **Mutex**: Implementação evitando deadlock usando mutex
- **Semaphore**: Implementação com semáforos para controle dos garfos
- **Monitor**: Implementação usando monitor para controle de estado

## Observações

- Todos os programas foram testados em ambiente Linux
- A biblioteca pthread é necessária para compilação
- Os programas usam funções POSIX, então podem não funcionar em sistemas Windows sem modificações
- Cada implementação possui mecanismos de prevenção de deadlock e starvation

## Problemas Comuns

1. **Erro de Compilação: pthread**

   ```bash
   # Solução
   gcc -o programa programa.c -pthread
   ```

2. **Permissão Negada ao Executar**

   ```bash
   # Solução
   chmod +x ./programa
   ```

3. **Biblioteca não Encontrada**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libc6-dev
   ```

## Contribuição

Sinta-se à vontade para contribuir com o projeto através de:

- Melhorias nos algoritmos
- Correções de bugs
- Documentação
- Novas implementações

## Licença

Este projeto está sob a licença MIT. Veja o arquivo LICENSE para mais detalhes.

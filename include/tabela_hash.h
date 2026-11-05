#ifndef TABELA_HASH_H
#define TABELA_HASH_H

/*
 * Tabela hash que mapeia strings (chaves) para inteiros (valores).
 *
 * - Enderecamento aberto com sondagem linear.
 * - Funcao de hash FNV-1a (32 bits).
 * - Capacidade sempre potencia de 2; a tabela e redimensionada quando
 *   (ocupados + lapides) / capacidade passaria de 0,75.
 * - Remocao com lapides (tombstones).
 * - A tabela e dona das chaves: cada chave e duplicada na insercao e
 *   liberada na remocao/destruicao.
 *
 * Complexidades "O(1) esperado" assumem hash bem distribuido e fator de
 * carga limitado; o pior caso teorico e O(n).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Tipo opaco: os campos ficam escondidos em src/tabela_hash.c. */
typedef struct TabelaHash TabelaHash;

/* Funcao chamada para cada par em th_percorrer (ordem nao especificada). */
typedef void (*THVisitante)(const char *chave, int valor, void *contexto);

/* Cria uma tabela com espaco para ao menos 'capacidade_inicial' pares sem
 * redimensionar (minimo 8 posicoes). Retorna NULL se faltar memoria. O(c) */
TabelaHash *th_criar(size_t capacidade_inicial);

/* Libera a tabela e todas as chaves. Aceita NULL. O(c) */
void th_destruir(TabelaHash *t);

/* Insere o par (chave, valor) ou atualiza o valor se a chave ja existir.
 * A chave e copiada. Retorna false se t/chave forem NULL ou faltar memoria
 * (nesse caso a tabela permanece como estava). O(1) esperado e amortizado */
bool th_inserir(TabelaHash *t, const char *chave, int valor);

/* Procura 'chave'; se existir, grava o valor em *saida (se saida != NULL)
 * e retorna true. O(1) esperado */
bool th_buscar(const TabelaHash *t, const char *chave, int *saida);

/* true se 'chave' existir na tabela. O(1) esperado */
bool th_contem(const TabelaHash *t, const char *chave);

/* Remove 'chave' (deixa uma lapide). Retorna false se nao existir. O(1) esperado */
bool th_remover(TabelaHash *t, const char *chave);

/* Numero de pares armazenados. O(1) */
size_t th_tamanho(const TabelaHash *t);

/* Numero de posicoes do vetor interno. O(1) */
size_t th_capacidade(const TabelaHash *t);

/* Chama visitante(chave, valor, contexto) para cada par. O(c) */
void th_percorrer(const TabelaHash *t, THVisitante visitante, void *contexto);

/* Funcao de hash FNV-1a de 32 bits (exposta para fins didaticos/testes). O(k) */
uint32_t th_fnv1a(const char *chave);

#endif /* TABELA_HASH_H */

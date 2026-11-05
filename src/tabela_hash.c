#include "tabela_hash.h"

#include <stdlib.h>
#include <string.h>

#define CAPACIDADE_MINIMA 8u

/* Fator de carga maximo = CARGA_NUM / CARGA_DEN = 0,75 (conta em inteiros). */
#define CARGA_NUM 3u
#define CARGA_DEN 4u

#define FNV_OFFSET 2166136261u
#define FNV_PRIMO 16777619u

typedef enum { VAZIO = 0, OCUPADO, LAPIDE } Estado;

typedef struct {
    char *chave;    /* copia de propriedade da tabela (NULL se nao OCUPADO) */
    uint32_t hash;  /* hash guardado: evita recalcular e acelera comparacoes */
    int valor;
    unsigned char estado;
} Slot;

struct TabelaHash {
    Slot *slots;
    size_t capacidade; /* potencia de 2 */
    size_t tamanho;    /* slots OCUPADO */
    size_t lapides;    /* slots LAPIDE */
};

/* Maior capacidade aceita: evita overflow em capacidade * sizeof(Slot)
 * e nas contas do fator de carga. */
#define CAPACIDADE_MAXIMA ((SIZE_MAX / sizeof(Slot)) / CARGA_DEN)

uint32_t th_fnv1a(const char *chave)
{
    uint32_t h = FNV_OFFSET;
    for (const unsigned char *p = (const unsigned char *)chave; *p != '\0'; p++) {
        h ^= *p;
        h *= FNV_PRIMO;
    }
    return h;
}

static char *duplicar_string(const char *s)
{
    size_t n = strlen(s) + 1;
    char *copia = malloc(n);
    if (copia != NULL) {
        memcpy(copia, s, n);
    }
    return copia;
}

/* Proxima posicao na sondagem linear (com volta ao inicio via mascara). */
static size_t proximo_indice(const TabelaHash *t, size_t i)
{
    return (i + 1) & (t->capacidade - 1);
}

/* Retorna o indice do slot que contem 'chave' ou SIZE_MAX se nao existir.
 * Lapides sao puladas; a busca termina no primeiro slot VAZIO. */
static size_t localizar(const TabelaHash *t, const char *chave, uint32_t hash)
{
    size_t i = hash & (t->capacidade - 1);
    while (t->slots[i].estado != VAZIO) {
        const Slot *s = &t->slots[i];
        if (s->estado == OCUPADO && s->hash == hash && strcmp(s->chave, chave) == 0) {
            return i;
        }
        i = proximo_indice(t, i);
    }
    return SIZE_MAX;
}

/* Primeiro slot livre (VAZIO ou LAPIDE) na sequencia de sondagem de 'hash'.
 * So deve ser usado quando se sabe que a chave nao esta na tabela. */
static size_t slot_livre(const TabelaHash *t, uint32_t hash)
{
    size_t i = hash & (t->capacidade - 1);
    while (t->slots[i].estado == OCUPADO) {
        i = proximo_indice(t, i);
    }
    return i;
}

/* Realoca o vetor com 'nova_cap' posicoes e reinsere os pares ocupados.
 * As chaves nao sao copiadas (apenas os ponteiros) e o hash guardado e
 * reaproveitado. As lapides desaparecem. */
static bool redimensionar(TabelaHash *t, size_t nova_cap)
{
    Slot *novos = calloc(nova_cap, sizeof *novos); /* calloc => tudo VAZIO */
    if (novos == NULL) {
        return false;
    }
    Slot *antigos = t->slots;
    size_t cap_antiga = t->capacidade;

    t->slots = novos;
    t->capacidade = nova_cap;
    t->lapides = 0;
    for (size_t i = 0; i < cap_antiga; i++) {
        if (antigos[i].estado == OCUPADO) {
            t->slots[slot_livre(t, antigos[i].hash)] = antigos[i];
        }
    }
    free(antigos);
    return true;
}

TabelaHash *th_criar(size_t capacidade_inicial)
{
    if (capacidade_inicial > CAPACIDADE_MAXIMA / 2) {
        return NULL;
    }
    /* Menor potencia de 2 que comporta capacidade_inicial com carga <= 0,75. */
    size_t cap = CAPACIDADE_MINIMA;
    while (cap * CARGA_NUM < capacidade_inicial * CARGA_DEN) {
        cap <<= 1;
    }

    TabelaHash *t = malloc(sizeof *t);
    if (t == NULL) {
        return NULL;
    }
    t->slots = calloc(cap, sizeof *t->slots);
    if (t->slots == NULL) {
        free(t);
        return NULL;
    }
    t->capacidade = cap;
    t->tamanho = 0;
    t->lapides = 0;
    return t;
}

void th_destruir(TabelaHash *t)
{
    if (t == NULL) {
        return;
    }
    for (size_t i = 0; i < t->capacidade; i++) {
        free(t->slots[i].chave); /* NULL para slots nao ocupados */
    }
    free(t->slots);
    free(t);
}

bool th_inserir(TabelaHash *t, const char *chave, int valor)
{
    if (t == NULL || chave == NULL) {
        return false;
    }
    uint32_t hash = th_fnv1a(chave);

    size_t i = localizar(t, chave, hash);
    if (i != SIZE_MAX) { /* chave existente: so atualiza */
        t->slots[i].valor = valor;
        return true;
    }

    /* Slots usados (ocupados + lapides) nao podem passar de 75%: as lapides
     * tambem alongam as sondagens. Se a maior parte for lapide, basta
     * reconstruir com a mesma capacidade; senao, a capacidade dobra. */
    if ((t->tamanho + t->lapides + 1) * CARGA_DEN > t->capacidade * CARGA_NUM) {
        size_t nova_cap = t->capacidade;
        if ((t->tamanho + 1) * 2 > t->capacidade) {
            if (t->capacidade > CAPACIDADE_MAXIMA / 2) {
                return false;
            }
            nova_cap = t->capacidade * 2;
        }
        if (!redimensionar(t, nova_cap)) {
            return false;
        }
    }

    char *copia = duplicar_string(chave);
    if (copia == NULL) {
        return false;
    }

    i = slot_livre(t, hash);
    if (t->slots[i].estado == LAPIDE) {
        t->lapides--; /* reaproveita a lapide */
    }
    t->slots[i].chave = copia;
    t->slots[i].hash = hash;
    t->slots[i].valor = valor;
    t->slots[i].estado = OCUPADO;
    t->tamanho++;
    return true;
}

bool th_buscar(const TabelaHash *t, const char *chave, int *saida)
{
    if (t == NULL || chave == NULL) {
        return false;
    }
    size_t i = localizar(t, chave, th_fnv1a(chave));
    if (i == SIZE_MAX) {
        return false;
    }
    if (saida != NULL) {
        *saida = t->slots[i].valor;
    }
    return true;
}

bool th_contem(const TabelaHash *t, const char *chave)
{
    return th_buscar(t, chave, NULL);
}

bool th_remover(TabelaHash *t, const char *chave)
{
    if (t == NULL || chave == NULL) {
        return false;
    }
    size_t i = localizar(t, chave, th_fnv1a(chave));
    if (i == SIZE_MAX) {
        return false;
    }

    free(t->slots[i].chave);
    t->slots[i].chave = NULL;
    t->tamanho--;

    if (t->slots[proximo_indice(t, i)].estado == VAZIO) {
        /* Nenhuma sequencia de sondagem passa por i (o proximo slot e VAZIO),
         * entao ele pode voltar a ser VAZIO, assim como as lapides que o
         * antecedem imediatamente. */
        t->slots[i].estado = VAZIO;
        size_t mascara = t->capacidade - 1;
        size_t j = (i + mascara) & mascara; /* indice anterior */
        while (t->slots[j].estado == LAPIDE) {
            t->slots[j].estado = VAZIO;
            t->lapides--;
            j = (j + mascara) & mascara;
        }
    } else {
        t->slots[i].estado = LAPIDE;
        t->lapides++;
    }
    return true;
}

size_t th_tamanho(const TabelaHash *t)
{
    return t != NULL ? t->tamanho : 0;
}

size_t th_capacidade(const TabelaHash *t)
{
    return t != NULL ? t->capacidade : 0;
}

void th_percorrer(const TabelaHash *t, THVisitante visitante, void *contexto)
{
    if (t == NULL || visitante == NULL) {
        return;
    }
    for (size_t i = 0; i < t->capacidade; i++) {
        if (t->slots[i].estado == OCUPADO) {
            visitante(t->slots[i].chave, t->slots[i].valor, contexto);
        }
    }
}

/*
 * Copyright (c) 2026 The mod_http3 Project Authors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <string.h>

#include "detail/quic_ngtcp2_impl.h"

#define QUIC_NGTCP2_MAP_MIN_CAP 16

static size_t map_hash(const uint8_t* key, size_t keylen)
{
    size_t h = 1469598103934665603u;
    for (size_t i = 0; i < keylen; i++)
    {
        h ^= key[i];
        h *= 1099511628211u;
    }
    return h;
}

static int map_same(const quic_ngtcp2_map_slot* slot, const uint8_t* key, size_t keylen)
{
    return slot->conn && slot->keylen == keylen && memcmp(slot->key, key, keylen) == 0;
}

/* Insert into a table known to have room, so it cannot fail or recurse. */
static void map_place(quic_ngtcp2_map_slot* slots, size_t cap, const uint8_t* key, size_t keylen, quic_ngtcp2_conn* conn)
{
    size_t i = map_hash(key, keylen) & (cap - 1);
    while (slots[i].conn && !map_same(&slots[i], key, keylen))
    {
        i = (i + 1) & (cap - 1);
    }
    memcpy(slots[i].key, key, keylen);
    slots[i].keylen = keylen;
    slots[i].conn = conn;
    slots[i].used = 1;
}

static int map_grow(quic_ngtcp2_map* map)
{
    size_t cap = map->cap ? map->cap * 2 : QUIC_NGTCP2_MAP_MIN_CAP;
    quic_ngtcp2_map_slot* slots = calloc(cap, sizeof(*slots));
    if (!slots)
    {
        return 0;
    }
    for (size_t i = 0; i < map->cap; i++)
    {
        if (map->slots[i].conn)
        {
            map_place(slots, cap, map->slots[i].key, map->slots[i].keylen, map->slots[i].conn);
        }
    }
    free(map->slots);
    map->slots = slots;
    map->cap = cap;
    return 1;
}

int quic_ngtcp2_map_set(quic_ngtcp2_map* map, const uint8_t* key, size_t keylen, quic_ngtcp2_conn* conn)
{
    QUIC_CHECK(map);
    QUIC_CHECK(key);
    if (keylen == 0 || keylen > NGTCP2_MAX_CIDLEN)
    {
        return 0;
    }
    /* Grow at three quarters, since linear probing degrades as the table fills. */
    if ((map->len + 1) * 4 >= map->cap * 3 && !map_grow(map))
    {
        return 0;
    }
    map_place(map->slots, map->cap, key, keylen, conn);
    map->len++;
    return 1;
}

quic_ngtcp2_conn* quic_ngtcp2_map_get(const quic_ngtcp2_map* map, const uint8_t* key, size_t keylen)
{
    QUIC_CHECK(map);
    if (!map->cap || keylen == 0 || keylen > NGTCP2_MAX_CIDLEN)
    {
        return NULL;
    }
    size_t i = map_hash(key, keylen) & (map->cap - 1);
    for (size_t probe = 0; probe < map->cap && map->slots[i].used; probe++)
    {
        if (map_same(&map->slots[i], key, keylen))
        {
            return map->slots[i].conn;
        }
        i = (i + 1) & (map->cap - 1);
    }
    return NULL;
}

void quic_ngtcp2_map_del(quic_ngtcp2_map* map, const uint8_t* key, size_t keylen)
{
    QUIC_CHECK(map);
    if (!map->cap || keylen == 0 || keylen > NGTCP2_MAX_CIDLEN)
    {
        return;
    }
    size_t i = map_hash(key, keylen) & (map->cap - 1);
    for (size_t probe = 0; probe < map->cap && map->slots[i].used; probe++)
    {
        if (map_same(&map->slots[i], key, keylen))
        {
            /* used stays set: clearing it would cut probe chains that run through here. */
            map->slots[i].conn = NULL;
            map->slots[i].keylen = 0;
            map->len--;
            return;
        }
        i = (i + 1) & (map->cap - 1);
    }
}

void quic_ngtcp2_map_free(quic_ngtcp2_map* map)
{
    if (map)
    {
        free(map->slots);
        map->slots = NULL;
        map->cap = 0;
        map->len = 0;
    }
}

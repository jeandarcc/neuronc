#include "graphics/assets/graphics_asset_internal.h"
#include "graphics/graphics_core_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  float *data;
  size_t count;
  size_t capacity;
} NeuronFloatVec;

typedef struct {
  uint32_t *data;
  size_t count;
  size_t capacity;
} NeuronU32Vec;

typedef struct {
  NeuronGraphicsVertex *data;
  size_t count;
  size_t capacity;
} NeuronVertexVec;

typedef struct {
  int position_index;
  int texcoord_index;
  int normal_index;
} NeuronObjFaceVertex;

static void neuron_float_vec_free(NeuronFloatVec *vec) {
  if (vec == NULL) {
    return;
  }
  free(vec->data);
  vec->data = NULL;
  vec->count = 0;
  vec->capacity = 0;
}

static int neuron_float_vec_push(NeuronFloatVec *vec, float value) {
  float *next_data = NULL;
  size_t next_capacity = 0;
  if (vec == NULL) {
    return 0;
  }

  if (vec->count == vec->capacity) {
    next_capacity = vec->capacity == 0 ? 32u : vec->capacity * 2u;
    next_data = (float *)realloc(vec->data, next_capacity * sizeof(float));
    if (next_data == NULL) {
      return 0;
    }
    vec->data = next_data;
    vec->capacity = next_capacity;
  }

  vec->data[vec->count++] = value;
  return 1;
}

static void neuron_u32_vec_free(NeuronU32Vec *vec) {
  if (vec == NULL) {
    return;
  }
  free(vec->data);
  vec->data = NULL;
  vec->count = 0;
  vec->capacity = 0;
}

static int neuron_u32_vec_push(NeuronU32Vec *vec, uint32_t value) {
  uint32_t *next_data = NULL;
  size_t next_capacity = 0;
  if (vec == NULL) {
    return 0;
  }

  if (vec->count == vec->capacity) {
    next_capacity = vec->capacity == 0 ? 64u : vec->capacity * 2u;
    next_data = (uint32_t *)realloc(vec->data, next_capacity * sizeof(uint32_t));
    if (next_data == NULL) {
      return 0;
    }
    vec->data = next_data;
    vec->capacity = next_capacity;
  }

  vec->data[vec->count++] = value;
  return 1;
}

static void neuron_vertex_vec_free(NeuronVertexVec *vec) {
  if (vec == NULL) {
    return;
  }
  free(vec->data);
  vec->data = NULL;
  vec->count = 0;
  vec->capacity = 0;
}

static int neuron_vertex_vec_push(NeuronVertexVec *vec,
                                  const NeuronGraphicsVertex *vertex) {
  NeuronGraphicsVertex *next_data = NULL;
  size_t next_capacity = 0;
  if (vec == NULL || vertex == NULL) {
    return 0;
  }

  if (vec->count == vec->capacity) {
    next_capacity = vec->capacity == 0 ? 32u : vec->capacity * 2u;
    next_data = (NeuronGraphicsVertex *)realloc(
        vec->data, next_capacity * sizeof(NeuronGraphicsVertex));
    if (next_data == NULL) {
      return 0;
    }
    vec->data = next_data;
    vec->capacity = next_capacity;
  }

  vec->data[vec->count++] = *vertex;
  return 1;
}

static char *neuron_obj_next_token(char *input, char **context) {
#if defined(_WIN32)
  return strtok_s(input, " \t\r\n", context);
#else
  return strtok_r(input, " \t\r\n", context);
#endif
}

static int neuron_parse_obj_index_component(const char *text, size_t item_count,
                                            int *out_index) {
  char *end = NULL;
  long parsed = 0;
  if (text == NULL || *text == '\0' || out_index == NULL) {
    return 0;
  }

  parsed = strtol(text, &end, 10);
  if (end == text) {
    return 0;
  }
  if (parsed < 0) {
    parsed = (long)item_count + parsed + 1;
  }
  if (parsed <= 0 || (size_t)parsed > item_count) {
    return 0;
  }

  *out_index = (int)(parsed - 1);
  return 1;
}

static int neuron_parse_face_vertex(const char *token, size_t position_count,
                                    size_t texcoord_count, size_t normal_count,
                                    NeuronObjFaceVertex *out_vertex) {
  char buffer[128];
  char *first = NULL;
  char *second = NULL;
  char *third = NULL;
  if (token == NULL || out_vertex == NULL) {
    return 0;
  }

  memset(out_vertex, 0, sizeof(*out_vertex));
  out_vertex->position_index = -1;
  out_vertex->texcoord_index = -1;
  out_vertex->normal_index = -1;

  strncpy(buffer, token, sizeof(buffer) - 1u);
  buffer[sizeof(buffer) - 1u] = '\0';
  first = buffer;
  second = strchr(first, '/');
  if (second != NULL) {
    *second++ = '\0';
    third = strchr(second, '/');
    if (third != NULL) {
      *third++ = '\0';
    }
  }

  if (!neuron_parse_obj_index_component(first, position_count,
                                        &out_vertex->position_index)) {
    return 0;
  }
  if (second != NULL && *second != '\0' &&
      !neuron_parse_obj_index_component(second, texcoord_count,
                                        &out_vertex->texcoord_index)) {
    return 0;
  }
  if (third != NULL && *third != '\0' &&
      !neuron_parse_obj_index_component(third, normal_count,
                                        &out_vertex->normal_index)) {
    return 0;
  }

  return 1;
}

static void neuron_fill_vertex_component(NeuronGraphicsVertex *vertex,
                                         const NeuronFloatVec *positions,
                                         const NeuronFloatVec *texcoords,
                                         const NeuronFloatVec *normals,
                                         const NeuronObjFaceVertex *face_vertex) {
  const size_t pos_offset = (size_t)face_vertex->position_index * 3u;
  vertex->px = positions->data[pos_offset + 0u];
  vertex->py = positions->data[pos_offset + 1u];
  vertex->pz = positions->data[pos_offset + 2u];

  if (face_vertex->texcoord_index >= 0) {
    const size_t uv_offset = (size_t)face_vertex->texcoord_index * 2u;
    vertex->u = texcoords->data[uv_offset + 0u];
    vertex->v = 1.0f - texcoords->data[uv_offset + 1u];
  }

  if (face_vertex->normal_index >= 0) {
    const size_t normal_offset = (size_t)face_vertex->normal_index * 3u;
    vertex->nx = normals->data[normal_offset + 0u];
    vertex->ny = normals->data[normal_offset + 1u];
    vertex->nz = normals->data[normal_offset + 2u];
  }
}

int neuron_graphics_load_obj_mesh(const char *path, NeuronGraphicsMesh *mesh) {
  FILE *file = NULL;
  char line[2048];
  NeuronFloatVec positions;
  NeuronFloatVec texcoords;
  NeuronFloatVec normals;
  NeuronVertexVec vertices;
  NeuronU32Vec indices;

  if (path == NULL || mesh == NULL) {
    neuron_graphics_set_error("Invalid OBJ load arguments");
    return 0;
  }

  memset(&positions, 0, sizeof(positions));
  memset(&texcoords, 0, sizeof(texcoords));
  memset(&normals, 0, sizeof(normals));
  memset(&vertices, 0, sizeof(vertices));
  memset(&indices, 0, sizeof(indices));

  file = fopen(path, "rb");
  if (file == NULL) {
    neuron_graphics_set_error("Failed to open mesh '%s'", path);
    return 0;
  }

  while (fgets(line, (int)sizeof(line), file) != NULL) {
    if (line[0] == 'v' && isspace((unsigned char)line[1])) {
      float x = 0.0f;
      float y = 0.0f;
      float z = 0.0f;
      if (sscanf(line + 2, "%f %f %f", &x, &y, &z) == 3) {
        if (!neuron_float_vec_push(&positions, x) ||
            !neuron_float_vec_push(&positions, y) ||
            !neuron_float_vec_push(&positions, z)) {
          neuron_graphics_set_error("Out of memory while parsing OBJ positions");
          goto fail;
        }
      }
      continue;
    }

    if (line[0] == 'v' && line[1] == 't' &&
        isspace((unsigned char)line[2])) {
      float u = 0.0f;
      float v = 0.0f;
      if (sscanf(line + 3, "%f %f", &u, &v) >= 2) {
        if (!neuron_float_vec_push(&texcoords, u) ||
            !neuron_float_vec_push(&texcoords, v)) {
          neuron_graphics_set_error("Out of memory while parsing OBJ texcoords");
          goto fail;
        }
      }
      continue;
    }

    if (line[0] == 'v' && line[1] == 'n' &&
        isspace((unsigned char)line[2])) {
      float x = 0.0f;
      float y = 0.0f;
      float z = 0.0f;
      if (sscanf(line + 3, "%f %f %f", &x, &y, &z) == 3) {
        if (!neuron_float_vec_push(&normals, x) ||
            !neuron_float_vec_push(&normals, y) ||
            !neuron_float_vec_push(&normals, z)) {
          neuron_graphics_set_error("Out of memory while parsing OBJ normals");
          goto fail;
        }
      }
      continue;
    }

    if (line[0] == 'f' && isspace((unsigned char)line[1])) {
      NeuronObjFaceVertex face_vertices[64];
      size_t face_count = 0;
      char *cursor = line + 2;
      char *ctx = NULL;
      char *token = neuron_obj_next_token(cursor, &ctx);
      while (token != NULL && face_count < 64u) {
        if (!neuron_parse_face_vertex(token, positions.count / 3u,
                                      texcoords.count / 2u, normals.count / 3u,
                                      &face_vertices[face_count])) {
          neuron_graphics_set_error("Failed to parse OBJ face vertex in '%s'",
                                    path);
          goto fail;
        }
        ++face_count;
        token = neuron_obj_next_token(NULL, &ctx);
      }

      if (face_count >= 3u) {
        size_t tri = 1u;
        while (tri + 1u < face_count) {
          size_t tri_indices[3];
          NeuronGraphicsVertex vertex;
          memset(&vertex, 0, sizeof(vertex));

          tri_indices[0] = 0u;
          tri_indices[1] = tri;
          tri_indices[2] = tri + 1u;

          for (size_t corner = 0; corner < 3u; ++corner) {
            memset(&vertex, 0, sizeof(vertex));
            neuron_fill_vertex_component(&vertex, &positions, &texcoords,
                                         &normals,
                                         &face_vertices[tri_indices[corner]]);
            if (!neuron_vertex_vec_push(&vertices, &vertex) ||
                !neuron_u32_vec_push(&indices, (uint32_t)(vertices.count - 1u))) {
              neuron_graphics_set_error(
                  "Out of memory while building OBJ mesh triangles");
              goto fail;
            }
          }
          ++tri;
        }
      }
    }
  }

  fclose(file);
  file = NULL;

  if (vertices.count == 0u) {
    neuron_graphics_set_error("OBJ '%s' has no drawable triangles", path);
    goto fail;
  }

  mesh->vertices = vertices.data;
  mesh->vertex_count = vertices.count;
  mesh->indices = indices.data;
  mesh->index_count = indices.count;

  neuron_float_vec_free(&positions);
  neuron_float_vec_free(&texcoords);
  neuron_float_vec_free(&normals);
  return 1;

fail:
  if (file != NULL) {
    fclose(file);
  }
  neuron_float_vec_free(&positions);
  neuron_float_vec_free(&texcoords);
  neuron_float_vec_free(&normals);
  neuron_vertex_vec_free(&vertices);
  neuron_u32_vec_free(&indices);
  return 0;
}

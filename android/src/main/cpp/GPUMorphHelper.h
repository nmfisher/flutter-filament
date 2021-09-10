/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FFilamentAsset.h"
#include "FFilamentInstance.h"
#include "filament/Texture.h"
#include "filament/Engine.h"
#include <math/vec4.h>

#include <tsl/robin_map.h>

#include <vector>

using namespace filament;



struct cgltf_node;
struct cgltf_mesh;
struct cgltf_primitive;

namespace gltfio {

    /**
     * Internal class that partitions lists of morph weights and maintains a cache of BufferObject
     * instances. This allows gltfio to support up to 255 morph targets.
     *
     * Each partition is associated with an unordered set of 4 (or fewer) morph target indices, which
     * we call the "primary indices" for that time slice.
     *
     * Animator has ownership over a single instance of MorphHelper, thus it is 1:1 with FilamentAsset.
     */
    class GPUMorphHelper {
    public:
        using Entity = utils::Entity;

        GPUMorphHelper(FFilamentAsset *asset, FFilamentInstance *inst);

        ~GPUMorphHelper();

        void applyWeights(Entity entity, float const *weights, size_t count) noexcept;

    private:
        struct GltfTarget {
            const void *bufferObject;
            uint32_t bufferSize;
            int morphTargetIndex;
            cgltf_attribute_type type;
        };

        struct GltfPrimitive {
            filament::VertexBuffer *vertexBuffer;
            uint8_t positions[4];
            uint8_t tangents[4];
            std::vector <GltfTarget> targets; // TODO: flatten this?
        };

        struct TableEntry {
            std::vector <GltfPrimitive> primitives; // TODO: flatten this?
        };

        uint32_t textureSize = 0;
        uint32_t numTargets = 0;
        uint32_t numVertices = 0;
        int numAttributes = 3;

        Texture *texture;
        MaterialInstance *materialInstance;
        uint32_t* indicesBuffer;

        void addPrimitive(cgltf_mesh const *mesh, int primitiveIndex, TableEntry *entry);

        void createTexture();

        void createMaterialInstance();

        tsl::robin_map <Entity, TableEntry> mMorphTable;
        FFilamentAsset *mAsset;
        const FFilamentInstance *mInstance;
    };
}
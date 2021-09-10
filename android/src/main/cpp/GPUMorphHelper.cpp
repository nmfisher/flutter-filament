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

#include "GPUMorphHelper.h"

#include <backend/BufferDescriptor.h>
#include <filament/BufferObject.h>
#include <filament/RenderableManager.h>
#include <filament/VertexBuffer.h>
#include <filamat/Package.h>
#include <filamat/MaterialBuilder.h>
#include <filament/Color.h>

#include "GltfEnums.h"
#include "TangentsJob.h"
#include <android/log.h>
#include <iostream>

using namespace filament;
using namespace filamat;
using namespace filament::math;
using namespace utils;
namespace gltfio {

    static constexpr uint8_t kUnused = 0xff;

    uint32_t computeBindingSize(const cgltf_accessor *accessor);

    uint32_t computeBindingSize(const cgltf_accessor *accessor);

    uint32_t computeBindingOffset(const cgltf_accessor *accessor);

    static const auto FREE_CALLBACK = [](void *mem, size_t s, void *) {
        __android_log_print(ANDROID_LOG_INFO, "MyTag", "PBD of size %d freed", s);
        free(mem);
    };

    GPUMorphHelper::GPUMorphHelper(FFilamentAsset *asset, FFilamentInstance *inst) : mAsset(asset),
                                                                                     mInstance(
                                                                                             inst) {
        NodeMap &sourceNodes = asset->isInstanced() ? asset->mInstances[0]->nodeMap
                                                    : asset->mNodeMap;
        for (auto pair : sourceNodes) {
            cgltf_node const *node = pair.first;
            cgltf_mesh const *mesh = node->mesh;
            if (mesh) {
                cgltf_size pc = mesh->primitives_count;
                cgltf_size wc = mesh->weights_count;
                for (cgltf_size pi = 0, count = mesh->primitives_count; pi < count; ++pi) {
                    addPrimitive(mesh, pi, &mMorphTable[pair.second]);
                }
            }
        }
        createTexture();
        createMaterialInstance();
    }

    GPUMorphHelper::~GPUMorphHelper() {
        auto engine = mAsset->mEngine;
    }

    /**
    * Creates a Texture
    * Ultimately we want to set the texture once, on initialization.
    * Thereafter, the weights will be updated
    */
    void GPUMorphHelper::createTexture() {
        auto &engine = *(mAsset->mEngine);
        //BufferObject* textureBuffer = BufferObject::Builder().size(textureSize).build(engine);
        float *const textureBuffer = (float *const) malloc(textureSize);
        uint32_t offset = 0;
        for (auto &entry : mMorphTable) {
            for (auto &prim : entry.second.primitives) {
                for (auto &target : prim.targets) {
                    if(target.type == cgltf_attribute_type_position || target.type == cgltf_attribute_type_normal) {
                        //memset(textureBuffer + offset, 0, target.bufferSize);
                        memcpy(textureBuffer + offset, target.bufferObject, target.bufferSize);
                        float*ptr = (float*)target.bufferObject;

                        for(int i = 0; i < int(target.bufferSize / sizeof(float)); i++) {
                            __android_log_print(ANDROID_LOG_INFO, "MyTag", "offset %d %f", offset, *(textureBuffer+offset+i));
                        }
                        offset += int(target.bufferSize / sizeof(float));
                    }
                }
            }
        }

        size_t position_size = 3 * sizeof(float);
        //size_t normal_size = sizeof(short4);
        size_t textureWidth = numTargets * numVertices; // the number of "pixels" to assign to the texture, where each pixel represents either a position or a normal vector
        assert(textureWidth == numTargets * 24);
        //assert(textureWidth * (position_size + normal_size) == textureSize);
        //assert(textureWidth * position_size == textureSize);
        texture = Texture::Builder()
                .width(24)
                .height(1)
                //
                // We use (1,textureWidth) rather than (textureWidth,1) because the underlying textureBuffer is row-major (where each row contains the vertices for a given morph target, and all morph targets are stacked vertically),
                // whereas Filament expects textures as column-major (? check this). This transposes the texture so we can correctly sample at (vertex*attribute, morphTarget).
                .depth(numTargets)
                .sampler(Texture::Sampler::SAMPLER_2D_ARRAY)
                .format(Texture::InternalFormat::RGB32F)
//                .levels(0x01)
                .build(engine);

        __android_log_print(ANDROID_LOG_INFO, "MyTag", "Expected size  %d width at level 0 %d height", Texture::PixelBufferDescriptor::computeDataSize(Texture::Format::RGB,
                                                                                                                             Texture::Type::FLOAT, 24, 1, 4), texture->getWidth(0),texture->getHeight(0));


        Texture::PixelBufferDescriptor descriptor(
                textureBuffer,
                textureSize,
                Texture::Format::RGB,
                Texture::Type::FLOAT,
                FREE_CALLBACK,
                nullptr);

/*        Texture::PixelBufferDescriptor descriptor(
                textureBuffer,
                textureSize,
                Texture::Format::RGB,
                Texture::Type::FLOAT,
                4, 0,0, 24,
                FREE_CALLBACK,
                nullptr); */

        texture->setImage(engine, 0, 0,0, 0, textureWidth, 1, 1, std::move(descriptor));
    }



    //.require(VertexAttribute::UV0)
    //.require(MaterialBuilder.VertexAttribute.CUSTOM0)

    void GPUMorphHelper::createMaterialInstance() {
        MaterialBuilder::init();
        MaterialBuilder builder = MaterialBuilder()
                .name("DefaultMaterial")
                .platform(MaterialBuilder::Platform::MOBILE)
                .targetApi(MaterialBuilder::TargetApi::ALL)
                .optimization(MaterialBuilderBase::Optimization::NONE)
                .shading(MaterialBuilder::Shading::LIT)
                .parameter(MaterialBuilder::UniformType::FLOAT3, "baseColor")
                .parameter(MaterialBuilder::UniformType::INT3, "dimensions")
                //.parameter(MaterialBuilder::UniformType::UINT, numIndices, MaterialBuilder::ParameterPrecision::DEFAULT, "vertexIndices")
                .parameter(MaterialBuilder::UniformType::FLOAT, 4, MaterialBuilder::ParameterPrecision::DEFAULT, "morphTargetWeights")
                .parameter(MaterialBuilder::SamplerType::SAMPLER_2D_ARRAY, MaterialBuilder::SamplerFormat::FLOAT, MaterialBuilder::ParameterPrecision::DEFAULT, "morphTargets")
                //.printShaders(true)
                //.generateDebugInfo(true)
                .material(R"SHADER(void material(inout MaterialInputs material) {
                              prepareMaterial(material);
                              material.baseColor.rgb = materialParams.baseColor;
                              //material.roughness = 0.65;
                              //material.metallic = 1.0;
                              //material.clearCoat = 1.0;
                          })SHADER")
                .materialVertex(R"SHADER(
                    vec3 getMorphTarget(int vertexIndex, int morphTargetIndex) {
                        // UV coordinates are normalized to [0,1), so we divide the current vertex index by the total number of vertices to find the correct coordinate for this vertex
                        vec3 uv = vec3(float(vertexIndex) / float(materialParams.dimensions.x), 0.0f, float(morphTargetIndex));
                        return texture(materialParams_morphTargets, uv).xyz;
                    }
                    void materialVertex(inout MaterialVertexInputs material) {

                        // for every morph target
                        for(int morphTargetIndex = 0; morphTargetIndex < materialParams.dimensions.z; morphTargetIndex++) {

                            // get the weight to apply
                            float weight = materialParams.morphTargetWeights[morphTargetIndex];

                            // get the ID of this vertex, which will be the x-offset of the position attribute in the texture sampler
                            int vertexId = getVertexIndex();

                            // get the position of the target for this vertex
                            vec3 morphTargetPosition = getMorphTarget(vertexId, morphTargetIndex);
                            // update the world position of this vertex
                            material.worldPosition.xyz += (weight * morphTargetPosition);

                            // increment the vertexID by half the size of the texture to get the x-offset of the normal (all positions stored in the first half, all normals stored in the latter)
                            vertexId += materialParams.dimensions.x / 2;

                            // get the normal of this target for this vertex
                            //vec3 morphTargetNormal = getMorphTarget(vertexId, morphTargetIndex);
                            //material.worldNormal += (weight * morphTargetNormal);
                        }
                    })SHADER");

        Package pkg = builder.build(mAsset->mEngine->getJobSystem());
        Material* material = Material::Builder().package(pkg.getData(), pkg.getSize())
                .build(*mAsset->mEngine);
        materialInstance = material->createInstance();
        materialInstance->setParameter("morphTargets", texture, TextureSampler());

        //materialInstance->setParameter("vertexIndices", indicesBuffer, numIndices);

        materialInstance->setParameter("baseColor", filament::math::float3{ 0.0f, 0.0f, 1.0f });
        materialInstance->setParameter("morphTargetWeights", filament::math::float4 { 0.0f, 1.0f, 1.0f, 1.0f });
        materialInstance->setParameter("dimensions", filament::math::int3 { numVertices * 2, 0, numTargets }); // multiply by two to account for position + normal
        Entity entity = mAsset->getFirstEntityByName("Cube");
        RenderableManager::Instance inst = mAsset->mEngine->getRenderableManager().getInstance(entity);
        mAsset->mEngine->getRenderableManager().setMaterialInstanceAt(inst, 0, materialInstance);

    }

    void GPUMorphHelper::applyWeights(Entity entity, float const *weights, size_t count) noexcept {
        //materialInstance->setParameter("morphTargetWeights", filament::math::float4 { 1.0f, 1.0f, 1.0f, 1.0f });
        materialInstance->setParameter("morphTargetWeights", filament::math::float4 { weights[0], 1.0f, 1.0f, 1.0f });
    }

    void
    GPUMorphHelper::addPrimitive(cgltf_mesh const *mesh, int primitiveIndex, TableEntry *entry) {
        auto &engine = *mAsset->mEngine;
        const cgltf_primitive &prim = mesh->primitives[primitiveIndex];
        /*cgltf_size maxIndex = 0;
        for(cgltf_size i = 0; i < prim.indices->count; i++  ) {
            maxIndex = max(cgltf_accessor_read_index(prim.indices, i), maxIndex);
        }
        //numIndices = maxIndex+1; */
        //numIndices = prim.indices->count;

        /*indicesBuffer = (uint32_t*)malloc(sizeof(unsigned int) * prim.indices->count);
        for(int i = 0; i < prim.indices->count; i++) {
            cgltf_size index = cgltf_accessor_read_index(prim.indices, i);
            __android_log_print(ANDROID_LOG_INFO, "MyTag", "Read index %d ", index);
            *(indicesBuffer+i) = (uint32_t)index;
        }*/

        numTargets += prim.targets_count;

        const auto &gltfioPrim = mAsset->mMeshCache.at(mesh)[primitiveIndex];
        VertexBuffer *vertexBuffer = gltfioPrim.vertices;
        numVertices += vertexBuffer->getVertexCount();

        entry->primitives.push_back({vertexBuffer});
        auto &morphHelperPrim = entry->primitives.back();

        for (int i = 0; i < 4; i++) {
            morphHelperPrim.positions[i] = gltfioPrim.morphPositions[i];
            morphHelperPrim.tangents[i] = gltfioPrim.morphTangents[i];
        }

        const cgltf_accessor *previous = nullptr;

        // for this primitive, iterate over every target
        for (int targetIndex = 0; targetIndex < prim.targets_count; targetIndex++) {
            const cgltf_morph_target &morphTarget = prim.targets[targetIndex];
            for (cgltf_size aindex = 0; aindex < morphTarget.attributes_count; aindex++) {
                const cgltf_attribute &attribute = morphTarget.attributes[aindex];
                const cgltf_accessor *accessor = attribute.data;
                const cgltf_attribute_type atype = attribute.type;
                if (atype == cgltf_attribute_type_tangent) {
                    continue;
                }
                if (atype == cgltf_attribute_type_normal) {
                    __android_log_print(ANDROID_LOG_INFO, "MyTag", "Doing normal");

                    // TODO: use JobSystem for this, like what we do for non-morph tangents.
                    TangentsJob job;
                    TangentsJob::Params params = {.in = {&prim, targetIndex}};
                    TangentsJob::run(&params);

                    if (params.out.results) {
                        const size_t size = params.out.vertexCount * sizeof(short4);
                        textureSize += size;
                        morphHelperPrim.targets.push_back(
                                {params.out.results, size, targetIndex, atype});
                        params.out.results = nullptr;
                    }
                    continue;
                }
                if (atype == cgltf_attribute_type_position) {
                    __android_log_print(ANDROID_LOG_INFO, "MyTag", "Doing position");
                    // All position attributes must have the same data type.
                    assert_invariant(
                            !previous || previous->component_type == accessor->component_type);
                    assert_invariant(!previous || previous->type == accessor->type);
                    previous = accessor;

                    // This should always be non-null, but don't crash if the glTF is malformed.
                    if (accessor->buffer_view) {
                        auto bufferData = (const uint8_t *) accessor->buffer_view->buffer->data;
                        assert_invariant(bufferData);
                        const uint8_t *data = computeBindingOffset(accessor) + bufferData;
                        const uint32_t size = computeBindingSize(accessor);
                        textureSize += size;
                        morphHelperPrim.targets.push_back({data, size, targetIndex, atype});
                    }
                }
            }
        }
    }
}


//VertexBuffer* vBuf = VertexBuffer::Builder()
//        .vertexCount(numVertices)
//        .bufferCount(numPrimitives)
//        .attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT4, 0)
//        .build(*engine);
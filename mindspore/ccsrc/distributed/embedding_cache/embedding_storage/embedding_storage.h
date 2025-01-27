/**
 * Copyright 2022 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MINDSPORE_CCSRC_DISTRIBUTED_EMBEDDING_CACHE_EMBEDDING_STORAGE_EMBEDDING_STORAGE_H_
#define MINDSPORE_CCSRC_DISTRIBUTED_EMBEDDING_CACHE_EMBEDDING_STORAGE_EMBEDDING_STORAGE_H_

#include <memory>

#include "distributed/embedding_cache/allocator.h"
#include "distributed/embedding_cache/cache_strategy/cache.h"
#include "distributed/persistent/storage/storage.h"
#include "runtime/device/device_address.h"
#include "include/backend/visible.h"

namespace mindspore {
namespace distributed {
namespace storage {
using mindspore::device::DeviceAddress;

// EmbeddingStorage is encapsulated within the Huge Embedding Table's lookup and update. It supports embeddingstorage
// query and modification of Embeddings, interaction between the host cache(for hot spot data) and SSD disk(for non-hot
// spot data), and preferential access to Embeddings in the host cache. If the corresponding element cannot be found in
// the host cache, then read the element from the SSD. Otherwise, if the host cache has insufficient space, the expired
// elements will automatically be evicted the to the SSD.
//
// It is stored in a key-value pair. For example, if the type of id of EmbeddingLookup is int and type of EmbeddingTable
// is float, you can use the instance as follow: EmbeddingStorage<int, float>.
template <typename KeyType, typename ValueType, typename Allocator = Allocator<uint8_t>>
class BACKEND_EXPORT EmbeddingStorage {
 public:
  // The general Allocator type used allocate host memory.
  using AllocatorType = typename std::allocator_traits<Allocator>::template rebind_alloc<uint8_t>;

  // The host cache type.
  using CacheType = Cache<KeyType, int>;

  EmbeddingStorage(int32_t embedding_key, size_t embedding_dim, size_t capacity, const Allocator &alloc = Allocator())
      : embedding_key_(embedding_key), embedding_dim_(embedding_dim), capacity_(capacity), alloc_(alloc) {}
  virtual ~EmbeddingStorage() = default;

  // Initialize the EmbeddingStorage, such as create cache and local file storage instance.
  // Parameter[in] `device_address`: The device address of the Embedding Table corresponding to the EmbeddingStorage.
  virtual void Initialize(const DeviceAddress *device_address);

  // Finalize the EmbeddingStorage, release allocated resource.
  virtual void Finalize() {}

  // Batch embeddings lookup operation.
  // Query Embeddings in the host cache first, if the corresponding element cannot be found in the host cache, then read
  // the element from the SSD and insert host cache.
  // Access an element of the cache generally affects the location or order of the elements in the cache, depending
  // on different cache strategies.
  //
  // Parameter[in] `keys`: The pointer pointing all keys which need to query.
  // Parameter[in] `key_num`: The number of keys which need to query.
  // Parameter[out] `values`: The output embeddings.
  // Return whether the function was successfully executed.
  virtual bool Get(const KeyType *keys, size_t key_num, ValueType *values) = 0;

  // Batch embeddings update/insert operation.
  // Update/Insert Embeddings in the host cache first, if the host cache has insufficient space, the expired elements
  // will automatically be evicted the to the SSD.
  // Update or Insert an element of the cache generally affects the location or order of the elements in the cache,
  // depending on different cache strategies.
  //
  // Parameter[in] `keys`: The pointer pointing all keys whose emebedding need to update.
  // Parameter[in] `key_num`: The number of keys whose emebedding need to update.
  // Parameter[in] `values`: Embeddings corresponding to all keys need to be updated.
  // Return whether the function was successfully executed.
  virtual bool Put(const KeyType *keys, size_t key_num, const ValueType *values) = 0;

 protected:
  // Allocate host memory use alloc_.
  // Parameter[in] `size`: The number of bytes to allocate for memory.
  // Return the pointer to the allocated memory.
  template <typename T>
  T *AllocateMemory(size_t size) {
    return reinterpret_cast<T *>(std::allocator_traits<AllocatorType>::allocate(alloc_, size));
  }

  // Free host memory use alloc_.
  // Parameter[in] `ptr`: The pointer need to free.
  void FreeMemory(void *ptr) {
    MS_EXCEPTION_IF_NULL(ptr);
    std::allocator_traits<AllocatorType>::deallocate(alloc_, reinterpret_cast<uint8_t *>(ptr), 0);
  }

  // The host cache used to record all hot spot embeddings.
  std::unique_ptr<CacheType> cache_;

  // The storage(such as local file) used to record all non-hot spot embeddings.
  std::unique_ptr<StorageBase> storage_;

  // The unique key for embedding table.
  int32_t embedding_key_;

  // The embedding size of a embedding vector.
  size_t embedding_dim_;

  // The capacity of embedding storage, the same as the maximum number of key-value pairs that can be saved.
  size_t capacity_;

  // The common allocator used to alloacte host memory.
  AllocatorType alloc_;
};
}  // namespace storage
}  // namespace distributed
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_DISTRIBUTED_EMBEDDING_CACHE_EMBEDDING_STORAGE_EMBEDDING_STORAGE_H_

/**
 * Copyright 2022 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License"){}
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

#include "distributed/embedding_cache/embedding_storage/dense_embedding_storage.h"
#include <memory>

namespace mindspore {
namespace distributed {
namespace storage {
// The index kInvalidIndex(-1) represents the index of cache miss key.
static constexpr int kInvalidIndex = -1;

template <typename KeyType, typename ValueType, typename Allocator>
void DenseEmbeddingStorage<KeyType, ValueType, Allocator>::Initialize(const DeviceAddress *device_address) {
  MS_EXCEPTION_IF_NULL(device_address);
  EmbeddingStorage<KeyType, ValueType, Allocator>::Initialize(device_address);
  embedding_param_ptr_ = reinterpret_cast<ValueType *>(device_address->GetMutablePtr());
  MS_EXCEPTION_IF_NULL(embedding_param_ptr_);
  empty_slots_.resize(this->capacity_);
  std::iota(empty_slots_.rbegin(), empty_slots_.rend(), 0);
}

template <typename KeyType, typename ValueType, typename Allocator>
void DenseEmbeddingStorage<KeyType, ValueType, Allocator>::Finalize() {
  empty_slots_.clear();
  embedding_param_ptr_ = nullptr;
}

template <typename KeyType, typename ValueType, typename Allocator>
bool DenseEmbeddingStorage<KeyType, ValueType, Allocator>::Get(const KeyType *keys, size_t key_num, ValueType *values) {
  MS_EXCEPTION_IF_NULL(keys);
  MS_EXCEPTION_IF_NULL(values);
  MS_EXCEPTION_IF_NULL(embedding_param_ptr_);

  // 1. Query cache to get the index of each key in the Tensor, update the positions of cache hit elements in the cache
  // (cache refresh), and count the keys of cache hit/miss.
  size_t cache_miss_cnt = 0;
  int *cache_miss_offsets = this->template AllocateMemory<int>(sizeof(int) * key_num);
  MS_EXCEPTION_IF_NULL(cache_miss_offsets);
  int *indices_in_cache = this->template AllocateMemory<int>(sizeof(int) * key_num);
  MS_EXCEPTION_IF_NULL(indices_in_cache);
  QueryCache(keys, key_num, cache_miss_offsets, &cache_miss_cnt, indices_in_cache);

  // 2. Copy the embeddings from cache to the returned values for cache hit keys.
  for (size_t i = 0; i < key_num; i++) {
    if (indices_in_cache[i] == kInvalidIndex) {
      continue;
    }
    auto ret = memcpy_s(values + this->embedding_dim_ * i, this->embedding_dim_ * sizeof(ValueType),
                        embedding_param_ptr_ + this->embedding_dim_ * indices_in_cache[i],
                        this->embedding_dim_ * sizeof(ValueType));
    if (ret != EOK) {
      MS_LOG(ERROR) << "Memcpy for output value failed, output index[" << i << "], errno[" << ret << "]";
      return false;
    }
  }

  if (cache_miss_cnt == 0) {
    return true;
  }

  // 3. Reserve space for cache miss keys in the cache (if there is enough space in the cache, then do nothing), write
  // the evicted element to persistent storage, and record the space in the cache, using the space in the cache first.
  RETURN_IF_FALSE_WITH_LOG(TryEvict(cache_miss_cnt), "Reserve space for miss keys failed.");

  // 4. Insert the cache miss elements into the cache from persistent storage, and copy them to the returned values.
  RETURN_IF_FALSE_WITH_LOG(InsertMissCacheFromStorage(keys, cache_miss_offsets, cache_miss_cnt, values),
                           "Insert the cache miss elements into the cache from persistent storage failed.");

  this->FreeMemory(indices_in_cache);
  this->FreeMemory(cache_miss_offsets);
  return true;
}

template <typename KeyType, typename ValueType, typename Allocator>
bool DenseEmbeddingStorage<KeyType, ValueType, Allocator>::Put(const KeyType *keys, size_t key_num,
                                                               const ValueType *values) {
  MS_EXCEPTION_IF_NULL(keys);
  MS_EXCEPTION_IF_NULL(values);
  MS_EXCEPTION_IF_NULL(embedding_param_ptr_);

  // 1. Query cache to get the index of each key in the Tensor, update the positions of cache hit elements in the cache
  // (cache refresh), and count the keys of cache hit/miss.
  size_t cache_miss_cnt = 0;
  int *cache_miss_offsets = this->template AllocateMemory<int>(sizeof(int) * key_num);
  MS_EXCEPTION_IF_NULL(cache_miss_offsets);
  int *indices_in_cache = this->template AllocateMemory<int>(sizeof(int) * key_num);
  MS_EXCEPTION_IF_NULL(indices_in_cache);
  QueryCache(keys, key_num, cache_miss_offsets, &cache_miss_cnt, indices_in_cache);

  // 2. Update the embedding value to the cache for cache hit keys.
  for (size_t i = 0; i < key_num; i++) {
    if (indices_in_cache[i] == kInvalidIndex) {
      continue;
    }
    auto ret = memcpy_s(embedding_param_ptr_ + this->embedding_dim_ * indices_in_cache[i],
                        this->embedding_dim_ * sizeof(ValueType), values + this->embedding_dim_ * i,
                        this->embedding_dim_ * sizeof(ValueType));
    if (ret != EOK) {
      MS_LOG(ERROR) << "Memcpy for inserting embedding value failed, output index[" << i << "], errno[" << ret << "]";
      return false;
    }
  }

  if (cache_miss_cnt == 0) {
    return true;
  }

  // 3. Reserve space for cache miss keys in the cache (if there is enough space in the cache, then do nothing), write
  // the evicted element to persistent storage, and record the space in the cache, using the space in the cache first.
  RETURN_IF_FALSE_WITH_LOG(TryEvict(cache_miss_cnt), "Reserve space for miss keys failed.");

  // 4. Insert the cache miss elements into the cache from host memory.
  RETURN_IF_FALSE_WITH_LOG(InsertMissCacheFromMemory(keys, cache_miss_offsets, cache_miss_cnt, values),
                           "Insert cache miss elements into cache from host memory failed.");

  this->FreeMemory(indices_in_cache);
  this->FreeMemory(cache_miss_offsets);
  return true;
}

template <typename KeyType, typename ValueType, typename Allocator>
void DenseEmbeddingStorage<KeyType, ValueType, Allocator>::QueryCache(const KeyType *keys, size_t key_num,
                                                                      int *cache_miss_offsets, size_t *cache_miss_cnt,
                                                                      int *indices_in_cache) const {
  MS_EXCEPTION_IF_NULL(keys);
  MS_EXCEPTION_IF_NULL(cache_miss_offsets);
  MS_EXCEPTION_IF_NULL(cache_miss_cnt);
  MS_EXCEPTION_IF_NULL(indices_in_cache);
  MS_EXCEPTION_IF_NULL(this->cache_);

  for (size_t i = 0; i < key_num; i++) {
    if (this->cache_->Exists(keys[i])) {
      indices_in_cache[i] = this->cache_->Get(keys[i]);
      continue;
    }

    // Set index kInvalidIndex(-1) to represent cache miss.
    indices_in_cache[i] = kInvalidIndex;
    // Record cache miss key's offset in all query keys.
    cache_miss_offsets[(*cache_miss_cnt)++] = i;
  }
}

template <typename KeyType, typename ValueType, typename Allocator>
bool DenseEmbeddingStorage<KeyType, ValueType, Allocator>::TryEvict(size_t reserve_size) {
  // 1. Try evict some non-hot data in cache to reserve space for elements that will be inserted into the cache.
  MS_EXCEPTION_IF_NULL(this->cache_);
  std::vector<CacheElement> evicted_elements;
  this->cache_->TryEvict(reserve_size, &evicted_elements);
  if (evicted_elements.size() > 0) {
    return true;
  }

  size_t evicted_keys_len = evicted_elements.size() * sizeof(KeyType);
  KeyType *evicted_keys = this->template AllocateMemory<KeyType>(evicted_keys_len);
  int *evicted_indices = this->template AllocateMemory<int>(evicted_elements.size() * sizeof(int));
  MS_EXCEPTION_IF_NULL(evicted_keys);
  MS_EXCEPTION_IF_NULL(evicted_indices);

  size_t evicted_cnt = 0;
  (void)std::for_each(evicted_elements.begin(), evicted_elements.end(), [&, this](const CacheElement &element) {
    evicted_keys[evicted_cnt] = element.first;
    evicted_indices[evicted_cnt++] = element.second;
  });

  // 2. Update empty slot recorder.
  (void)empty_slots_.insert(empty_slots_.end(), evicted_indices, evicted_indices + evicted_elements.size());

  // 3. Get all evicted embedding vector values.
  size_t evicted_values_len = evicted_elements.size() * this->embedding_dim_ * sizeof(ValueType);
  ValueType *evicted_values = this->template AllocateMemory<ValueType>(evicted_values_len);
  MS_EXCEPTION_IF_NULL(evicted_values);
  MS_EXCEPTION_IF_NULL(embedding_param_ptr_);
  for (size_t i = 0; i < evicted_elements.size(); i++) {
    auto ret = memcpy_s(evicted_values + this->embedding_dim_ * i, this->embedding_dim_ * sizeof(ValueType),
                        embedding_param_ptr_ + this->embedding_dim_ * evicted_indices[i],
                        this->embedding_dim_ * sizeof(ValueType));
    if (ret != EOK) {
      MS_LOG(ERROR) << "Memcpy for evicted value failed, errno[" << ret << "]";
      return false;
    }
  }
  // 4. Write evicted elements to persistent storage.
  MS_EXCEPTION_IF_NULL(this->storage_);
  this->storage_->Write({evicted_keys, evicted_keys_len}, {evicted_values, evicted_values_len});

  this->FreeMemory(evicted_keys);
  this->FreeMemory(evicted_indices);
  this->FreeMemory(evicted_values);

  return true;
}

template <typename KeyType, typename ValueType, typename Allocator>
bool DenseEmbeddingStorage<KeyType, ValueType, Allocator>::InsertMissCacheFromStorage(const KeyType *keys,
                                                                                      const int *cache_miss_offsets,
                                                                                      size_t cache_miss_cnt,
                                                                                      ValueType *values) {
  MS_EXCEPTION_IF_NULL(keys);
  MS_EXCEPTION_IF_NULL(cache_miss_offsets);
  MS_EXCEPTION_IF_NULL(values);
  MS_EXCEPTION_IF_NULL(this->cache_);
  MS_EXCEPTION_IF_NULL(embedding_param_ptr_);
  if (empty_slots_.size() < cache_miss_cnt) {
    MS_LOG(EXCEPTION) << "There is no enough empty slot in cache.";
  }

  // 1. Read the cache miss element from the persistent storage.
  size_t cache_miss_keys_len = cache_miss_cnt * sizeof(KeyType);
  KeyType *cache_miss_keys = this->template AllocateMemory<KeyType>(cache_miss_keys_len);
  MS_EXCEPTION_IF_NULL(cache_miss_keys);
  for (size_t i = 0; i < cache_miss_cnt; i++) {
    cache_miss_keys[i] = keys[cache_miss_offsets[i]];
  }
  size_t cache_miss_values_len = cache_miss_cnt * this->embedding_dim_ * sizeof(ValueType);
  ValueType *cache_miss_values = this->template AllocateMemory<ValueType>(cache_miss_values_len);
  MS_EXCEPTION_IF_NULL(cache_miss_values);

  // Read the persistent storage.
  MS_EXCEPTION_IF_NULL(this->storage_);
  this->storage_->Read({cache_miss_keys, cache_miss_keys_len}, {cache_miss_values, cache_miss_values_len});

  // 2. Insert the cache miss elements into cache, and copy them to the returned values.
  for (size_t i = 0; i < cache_miss_cnt; i++) {
    int insert_index = empty_slots_[empty_slots_.size() - i - 1];
    // Insert key-index pairs of the cache miss elements into the cache.
    this->cache_->Put(cache_miss_keys[i], insert_index);

    // Copy the embedding vectors of cache miss elements to the cache.
    auto ret =
      memcpy_s(embedding_param_ptr_ + this->embedding_dim_ * insert_index, this->embedding_dim_ * sizeof(ValueType),
               cache_miss_values + this->embedding_dim_ * i, this->embedding_dim_ * sizeof(ValueType));
    if (ret != EOK) {
      MS_LOG(ERROR) << "Memcpy the embedding vectors of cache miss elements to the cache failed, errno[" << ret << "]";
      return false;
    }

    // Copy the embedding vectors of cache miss elements to the returned values.
    ret = memcpy_s(values + this->embedding_dim_ * cache_miss_offsets[i], this->embedding_dim_ * sizeof(ValueType),
                   cache_miss_values + this->embedding_dim_ * i, this->embedding_dim_ * sizeof(ValueType));
    if (ret != EOK) {
      MS_LOG(ERROR) << "Memcpy the embedding vectors of cache miss elements to the returned values failed, errno["
                    << ret << "]";
      return false;
    }
  }

  // 3. Update empty slots recorder.
  size_t remain_empty_slot_num = empty_slots_.size() - cache_miss_cnt;
  (void)empty_slots_.erase(empty_slots_.begin() + remain_empty_slot_num, empty_slots_.end());

  this->FreeMemory(cache_miss_keys);
  this->FreeMemory(cache_miss_values);
  return true;
}

template <typename KeyType, typename ValueType, typename Allocator>
bool DenseEmbeddingStorage<KeyType, ValueType, Allocator>::InsertMissCacheFromMemory(const KeyType *keys,
                                                                                     const int *cache_miss_offsets,
                                                                                     size_t cache_miss_cnt,
                                                                                     const ValueType *values) {
  MS_EXCEPTION_IF_NULL(keys);
  MS_EXCEPTION_IF_NULL(cache_miss_offsets);
  MS_EXCEPTION_IF_NULL(values);
  MS_EXCEPTION_IF_NULL(this->cache_);
  MS_EXCEPTION_IF_NULL(embedding_param_ptr_);

  if (empty_slots_.size() < cache_miss_cnt) {
    MS_LOG(EXCEPTION) << "There is no enough empty slot in cache.";
  }

  // 1. Insert the cache miss elements into cache.
  for (size_t i = 0; i < cache_miss_cnt; i++) {
    int insert_index = empty_slots_[empty_slots_.size() - i - 1];
    // Insert key-index pairs of the cache miss elements into the cache.
    this->cache_->Put(keys[cache_miss_offsets[i]], insert_index);

    // Copy the embedding vectors of cache miss elements to the cache.
    auto ret =
      memcpy_s(embedding_param_ptr_ + this->embedding_dim_ * insert_index, this->embedding_dim_ * sizeof(ValueType),
               values + this->embedding_dim_ * cache_miss_offsets[i], this->embedding_dim_ * sizeof(ValueType));
    if (ret != EOK) {
      MS_LOG(ERROR) << "Memcpy the embedding vectors of cache miss elements to the cache failed, errno[" << ret << "]";
      return false;
    }
  }

  // 2. Update empty slots recorder.
  size_t remain_empty_slot_num = empty_slots_.size() - cache_miss_cnt;
  (void)empty_slots_.erase(empty_slots_.begin() + remain_empty_slot_num, empty_slots_.end());
  return true;
}

template class DenseEmbeddingStorage<int32_t, bool>;
template class DenseEmbeddingStorage<int32_t, int8_t>;
template class DenseEmbeddingStorage<int32_t, int16_t>;
template class DenseEmbeddingStorage<int32_t, int32_t>;
template class DenseEmbeddingStorage<int32_t, int64_t>;
template class DenseEmbeddingStorage<int32_t, uint8_t>;
template class DenseEmbeddingStorage<int32_t, uint16_t>;
template class DenseEmbeddingStorage<int32_t, uint32_t>;
template class DenseEmbeddingStorage<int32_t, uint64_t>;
template class DenseEmbeddingStorage<int32_t, float16>;
template class DenseEmbeddingStorage<int32_t, float>;
template class DenseEmbeddingStorage<int32_t, double>;

template class DenseEmbeddingStorage<int64_t, bool>;
template class DenseEmbeddingStorage<int64_t, int8_t>;
template class DenseEmbeddingStorage<int64_t, int16_t>;
template class DenseEmbeddingStorage<int64_t, int32_t>;
template class DenseEmbeddingStorage<int64_t, int64_t>;
template class DenseEmbeddingStorage<int64_t, uint8_t>;
template class DenseEmbeddingStorage<int64_t, uint16_t>;
template class DenseEmbeddingStorage<int64_t, uint32_t>;
template class DenseEmbeddingStorage<int64_t, uint64_t>;
template class DenseEmbeddingStorage<int64_t, float16>;
template class DenseEmbeddingStorage<int64_t, float>;
template class DenseEmbeddingStorage<int64_t, double>;

template class DenseEmbeddingStorage<int32_t, float, std::allocator<uint8_t>>;
template class DenseEmbeddingStorage<int64_t, float, std::allocator<uint8_t>>;
}  // namespace storage
}  // namespace distributed
}  // namespace mindspore

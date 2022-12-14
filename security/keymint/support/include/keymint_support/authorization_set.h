/*
 * Copyright 2020 The Android Open Source Project
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

#pragma once

#include <vector>

#include <aidl/android/hardware/security/keymint/BlockMode.h>
#include <aidl/android/hardware/security/keymint/Digest.h>
#include <aidl/android/hardware/security/keymint/EcCurve.h>
#include <aidl/android/hardware/security/keymint/PaddingMode.h>

#include <keymint_support/keymint_tags.h>

namespace aidl::android::hardware::security::keymint {

using std::vector;

class AuthorizationSetBuilder;

/**
 * A collection of KeyParameters. It provides memory ownership and some convenient functionality for
 * sorting, deduplicating, joining, and subtracting sets of KeyParameters.
 */
class AuthorizationSet {
  public:
    typedef KeyParameter value_type;

    /**
     * Construct an empty, dynamically-allocated, growable AuthorizationSet.
     */
    AuthorizationSet(){};

    // Copy constructor.
    AuthorizationSet(const AuthorizationSet& other) : data_(other.data_) {}

    // Move constructor.
    AuthorizationSet(AuthorizationSet&& other) noexcept : data_(std::move(other.data_)) {}

    // Constructor from vector<KeyParameter>
    AuthorizationSet(const vector<KeyParameter>& other) { *this = other; }

    // Copy assignment.
    AuthorizationSet& operator=(const AuthorizationSet& other) {
        data_ = other.data_;
        return *this;
    }

    // Move assignment.
    AuthorizationSet& operator=(AuthorizationSet&& other) noexcept {
        data_ = std::move(other.data_);
        return *this;
    }

    AuthorizationSet& operator=(const vector<KeyParameter>& other) {
        if (other.size() > 0) {
            data_.resize(other.size());
            for (size_t i = 0; i < data_.size(); ++i) {
                /* This makes a deep copy even of embedded blobs.
                 * See assignment operator/copy constructor of vector.*/
                data_[i] = other[i];
            }
        }
        return *this;
    }

    /**
     * Clear existing authorization set data
     */
    void Clear();

    ~AuthorizationSet() = default;

    /**
     * Returns the size of the set.
     */
    size_t size() const { return data_.size(); }

    /**
     * Returns true if the set is empty.
     */
    bool empty() const { return size() == 0; }

    /**
     * Returns the data in the set, directly. Be careful with this.
     */
    const KeyParameter* data() const { return data_.data(); }

    /**
     * Sorts the set
     */
    void Sort();

    /**
     * Sorts the set and removes duplicates (inadvertently duplicating tags is easy to do with the
     * AuthorizationSetBuilder).
     */
    void Deduplicate();

    /**
     * Adds all elements from \p set that are not already present in this AuthorizationSet.  As a
     * side-effect, if \p set is not null this AuthorizationSet will end up sorted.
     */
    void Union(const AuthorizationSet& set);

    /**
     * Removes all elements in \p set from this AuthorizationSet.
     */
    void Subtract(const AuthorizationSet& set);

    /**
     * Returns the offset of the next entry that matches \p tag, starting from the element after \p
     * begin.  If not found, returns -1.
     */
    int find(Tag tag, int begin = -1) const;

    /**
     * Removes the entry at the specified index. Returns true if successful, false if the index was
     * out of bounds.
     */
    bool erase(int index);

    /**
     * Returns iterator (pointer) to beginning of elems array, to enable STL-style iteration
     */
    auto begin() { return data_.begin(); }
    auto begin() const { return data_.begin(); }

    /**
     * Returns iterator (pointer) one past end of elems array, to enable STL-style iteration
     */
    auto end() { return data_.end(); }
    auto end() const { return data_.end(); }

    /**
     * Returns the nth element of the set.
     * Like for std::vector::operator[] there is no range check performed. Use of out of range
     * indices is undefined.
     */
    KeyParameter& operator[](int n);

    /**
     * Returns the nth element of the set.
     * Like for std::vector::operator[] there is no range check performed. Use of out of range
     * indices is undefined.
     */
    const KeyParameter& operator[](int n) const;

    /**
     * Returns true if the set contains at least one instance of \p tag
     */
    bool Contains(Tag tag) const { return find(tag) != -1; }

    template <TagType tag_type, Tag tag, typename ValueT>
    bool Contains(TypedTag<tag_type, tag> ttag, const ValueT& value) const {
        for (const auto& param : data_) {
            auto entry = authorizationValue(ttag, param);
            if (entry && static_cast<ValueT>(*entry) == value) return true;
        }
        return false;
    }
    /**
     * Returns the number of \p tag entries.
     */
    size_t GetTagCount(Tag tag) const;

    template <typename T>
    inline auto GetTagValue(T tag) const -> decltype(authorizationValue(tag, KeyParameter())) {
        auto entry = GetEntry(tag);
        if (entry) return authorizationValue(tag, *entry);
        return {};
    }

    void push_back(const KeyParameter& param) { data_.push_back(param); }
    void push_back(KeyParameter&& param) { data_.push_back(std::move(param)); }
    void push_back(const AuthorizationSet& set) {
        for (auto& entry : set) {
            push_back(entry);
        }
    }
    void push_back(AuthorizationSet&& set) {
        std::move(set.begin(), set.end(), std::back_inserter(*this));
    }

    /**
     * Append the tag and enumerated value to the set.
     * "val" may be exactly one parameter unless a boolean parameter is added.
     * In this case "val" is omitted. This condition is checked at compile time by Authorization()
     */
    template <typename TypedTagT, typename... Value>
    void push_back(TypedTagT tag, Value&&... val) {
        push_back(Authorization(tag, std::forward<Value>(val)...));
    }

    template <typename Iterator>
    void append(Iterator begin, Iterator end) {
        while (begin != end) {
            push_back(*begin);
            ++begin;
        }
    }

    vector<KeyParameter> vector_data() const {
        vector<KeyParameter> result(begin(), end());
        return result;
    }

  private:
    std::optional<std::reference_wrapper<const KeyParameter>> GetEntry(Tag tag) const;

    std::vector<KeyParameter> data_;
};

class AuthorizationSetBuilder : public AuthorizationSet {
  public:
    template <typename TagType, typename... ValueType>
    AuthorizationSetBuilder& Authorization(TagType ttag, ValueType&&... value) {
        push_back(ttag, std::forward<ValueType>(value)...);
        return *this;
    }

    template <Tag tag>
    AuthorizationSetBuilder& Authorization(TypedTag<TagType::BYTES, tag> ttag, const uint8_t* data,
                                           size_t data_length) {
        vector<uint8_t> new_blob(data, data + data_length);
        push_back(ttag, new_blob);
        return *this;
    }

    template <Tag tag>
    AuthorizationSetBuilder& Authorization(TypedTag<TagType::BYTES, tag> ttag, const char* data,
                                           size_t data_length) {
        return Authorization(ttag, reinterpret_cast<const uint8_t*>(data), data_length);
    }

    template <Tag tag>
    AuthorizationSetBuilder& Authorization(TypedTag<TagType::BYTES, tag> ttag, char* data,
                                           size_t data_length) {
        return Authorization(ttag, reinterpret_cast<const uint8_t*>(data), data_length);
    }

    template <Tag tag, size_t size>
    AuthorizationSetBuilder& Authorization(TypedTag<TagType::BYTES, tag> ttag,
                                           const char (&data)[size]) {
        return Authorization(ttag, reinterpret_cast<const uint8_t*>(&data[0]),
                             size - 1);  // drop the terminating '\0'
    }

    template <Tag tag>
    AuthorizationSetBuilder& Authorization(TypedTag<TagType::BYTES, tag> ttag,
                                           const std::string& data) {
        return Authorization(ttag, reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }

    AuthorizationSetBuilder& Authorizations(const AuthorizationSet& set) {
        for (const auto& entry : set) {
            push_back(entry);
        }
        return *this;
    }

    AuthorizationSetBuilder& RsaKey(uint32_t key_size, uint64_t public_exponent);
    AuthorizationSetBuilder& EcdsaKey(uint32_t key_size);
    AuthorizationSetBuilder& EcdsaKey(EcCurve curve);
    AuthorizationSetBuilder& AesKey(uint32_t key_size);
    AuthorizationSetBuilder& TripleDesKey(uint32_t key_size);
    AuthorizationSetBuilder& HmacKey(uint32_t key_size);

    AuthorizationSetBuilder& RsaSigningKey(uint32_t key_size, uint64_t public_exponent);
    AuthorizationSetBuilder& RsaEncryptionKey(uint32_t key_size, uint64_t public_exponent);
    AuthorizationSetBuilder& EcdsaSigningKey(EcCurve curve);
    AuthorizationSetBuilder& AesEncryptionKey(uint32_t key_size);
    AuthorizationSetBuilder& TripleDesEncryptionKey(uint32_t key_size);

    AuthorizationSetBuilder& SigningKey();
    AuthorizationSetBuilder& EncryptionKey();
    AuthorizationSetBuilder& AttestKey();

    AuthorizationSetBuilder& NoDigestOrPadding();

    AuthorizationSetBuilder& EcbMode();
    AuthorizationSetBuilder& GcmModeMinMacLen(uint32_t minMacLength);
    AuthorizationSetBuilder& GcmModeMacLen(uint32_t macLength);

    AuthorizationSetBuilder& BlockMode(std::initializer_list<BlockMode> blockModes);
    AuthorizationSetBuilder& OaepMGFDigest(const std::vector<Digest>& digests);
    AuthorizationSetBuilder& Digest(std::vector<Digest> digests);
    AuthorizationSetBuilder& Padding(std::initializer_list<PaddingMode> paddings);

    AuthorizationSetBuilder& SetDefaultValidity();

    AuthorizationSetBuilder& AttestationChallenge(const std::string& challenge) {
        return Authorization(TAG_ATTESTATION_CHALLENGE, challenge);
    }
    AuthorizationSetBuilder& AttestationChallenge(std::vector<uint8_t> challenge) {
        return Authorization(TAG_ATTESTATION_CHALLENGE, challenge);
    }

    AuthorizationSetBuilder& AttestationApplicationId(const std::string& id) {
        return Authorization(TAG_ATTESTATION_APPLICATION_ID, id);
    }
    AuthorizationSetBuilder& AttestationApplicationId(std::vector<uint8_t> id) {
        return Authorization(TAG_ATTESTATION_APPLICATION_ID, id);
    }

    template <typename... T>
    AuthorizationSetBuilder& BlockMode(T&&... a) {
        return BlockMode({std::forward<T>(a)...});
    }
    template <typename... T>
    AuthorizationSetBuilder& Digest(T&&... a) {
        return Digest({std::forward<T>(a)...});
    }
    template <typename... T>
    AuthorizationSetBuilder& Padding(T&&... a) {
        return Padding({std::forward<T>(a)...});
    }
};

}  // namespace aidl::android::hardware::security::keymint

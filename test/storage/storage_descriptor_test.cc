// Copyright (c) 2013, Per Eckerdal. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "gtest/gtest.h"

#include "storage/storage_descriptor.h"

namespace {

class MockGCHooks {
 public:
  MockGCHooks() = delete;

  inline static void* read(void** ptr) {
    return *ptr;
  }

  inline static void write(void** ptr, void* value) {
    *ptr = value;
  }
};

typedef arw::StorageDescriptor<MockGCHooks> SD;

template<typename Iter>
static std::unique_ptr<SD>
makeSD(size_t sizeWithEmptyArray,
       bool hasArray,
       const SD::Slot& array,
       Iter valuesBegin,
       Iter valuesEnd) {
  SD* obj = reinterpret_cast<SD*>(
    new char[SD::objectSize(valuesEnd-valuesBegin)]);
  return std::unique_ptr<SD>(SD::init(obj,
                                      sizeWithEmptyArray,
                                      hasArray,
                                      array,
                                      valuesBegin,
                                      valuesEnd));
}

static std::unique_ptr<SD> makeVariableSizeSD() {
  return makeSD(
    /*sizeWithEmptyArray:*/0,
    /*hasArray:*/true,
    /*array:*/SD::Slot(NULL, arw::HandleType::REFERENCE, 123),
    /*valuesBegin:*/static_cast<SD::Slot*>(0),
    /*valuesEnd:*/static_cast<SD::Slot*>(0));
}

static std::unique_ptr<SD> makeSDWithNoSlots() {
  return makeSD(
    /*sizeWithEmptyArray:*/0,
    /*hasArray:*/true,
    /*array:*/SD::Slot(NULL, arw::HandleType::REFERENCE, 0),
    /*valuesBegin:*/static_cast<SD::Slot*>(nullptr),
    /*valuesEnd:*/static_cast<SD::Slot*>(nullptr));
}

static std::unique_ptr<SD> makeSDWithOneSlot() {
  const SD::Slot slot(NULL, arw::HandleType::REFERENCE, 5);
  return makeSD(
    /*sizeWithEmptyArray:*/0,
    /*hasArray:*/true,
    /*array:*/SD::Slot(NULL, arw::HandleType::REFERENCE, 0),
    /*valuesBegin:*/&slot,
    /*valuesEnd:*/(&slot)+1);
}

}  // namespace


TEST(StorageDescriptor, Slot) {
  const SD::Slot slot(NULL, arw::HandleType::REFERENCE, 5);
  EXPECT_TRUE(SD::Slot::empty.storageDescriptor.get() ==
              slot.storageDescriptor.get());
  EXPECT_EQ(5, slot.offset);
  EXPECT_TRUE(arw::HandleType::REFERENCE == slot.type);


  // Check that we're not allowed to create by-value boxed types
  EXPECT_DEATH(SD::Slot(NULL, arw::HandleType::VALUE, 0), "");

  // Check that we're not allowed to create by-value slots for variable-size
  // objects.
  const std::unique_ptr<SD> sd = makeVariableSizeSD();
  EXPECT_DEATH(SD::Slot(sd.get(), arw::HandleType::VALUE, 0), "");
}

TEST(StorageDescriptor, Accessors) {
  const std::unique_ptr<SD> sd = makeVariableSizeSD();
  SD::Slot boxedSlot(NULL, arw::HandleType::REFERENCE, 5);

  EXPECT_EQ(sizeof(SD)+sizeof(SD::Slot)*5, SD::objectSize(5));
  EXPECT_TRUE(sd->hasArray());
  EXPECT_TRUE(!sd->isBoxed());
  EXPECT_TRUE(boxedSlot.storageDescriptor->isBoxed());
}

TEST(StorageDescriptor, Construct) {
  // Construct with one field
  const SD::Slot slot(NULL, arw::HandleType::REFERENCE, 5);
  makeSD(
    /*sizeWithEmptyArray:*/0,
    /*hasArray:*/true,
    /*array:*/SD::Slot(NULL, arw::HandleType::REFERENCE, 0),
    /*valuesBegin:*/&slot,
    /*valuesEnd:*/(&slot)+1);
}

TEST(StorageDescriptor, ArrayAccessor) {
  const std::unique_ptr<SD> sd = makeVariableSizeSD();
  EXPECT_EQ(sd->array().type, arw::HandleType::REFERENCE);
  EXPECT_EQ(sd->array().offset, 123);
  EXPECT_TRUE(sd->array().storageDescriptor->isBoxed());
}

TEST(StorageDescriptor, IterateWithNoSlots) {
  const auto sd = makeSDWithNoSlots();
  EXPECT_TRUE(sd->begin() == sd->end());
  EXPECT_FALSE(sd->begin() != sd->end());
}

TEST(StorageDescriptor, IteratingWithForLoop) {
  const auto sd = makeSDWithOneSlot();
  size_t count = 0;
  for (const auto &slot : *sd) {
    count++;
  }
  EXPECT_EQ(count, 1);
}

TEST(StorageDescriptor, IterateAndReachEnd) {
  const auto sd = makeSDWithOneSlot();
  EXPECT_FALSE(sd->begin() == sd->end());
  EXPECT_TRUE(sd->begin() != sd->end());

  EXPECT_TRUE(++sd->begin() == sd->end());
  EXPECT_FALSE(++sd->begin() != sd->end());
}

TEST(StorageDescriptor, IteratorPostIncrement) {
  const auto sd = makeSDWithOneSlot();
  auto it = sd->begin();

  EXPECT_TRUE((it++) == sd->begin());
  EXPECT_TRUE(it == sd->end());
}

TEST(StorageDescriptor, IteratorMemberAccess) {
  const auto sd = makeSDWithOneSlot();
  const auto it = sd->begin();
  EXPECT_EQ(it->offset, 5);
  EXPECT_EQ(it->type, arw::HandleType::REFERENCE);
}

TEST(StorageDescriptor, IteratorDereference) {
  const auto sd = makeSDWithOneSlot();
  const auto it = sd->begin();
  EXPECT_EQ((*it).offset, 5);
  EXPECT_EQ((*it).type, arw::HandleType::REFERENCE);
}

// TODO(peck): Verify that read barrier is invoked when dereferencing
//     const_iterator and calling array().

#include "gtest/gtest.h"
#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"


static const int32_t kOctave = 12 << 7;

class QuantizerTest : public ::testing::Test {
public:
  virtual void SetUp() {
    quantizer_.Init();
  }

  virtual void TearDown() {

  }
protected:

  braids::Quantizer quantizer_;
};

TEST_F(QuantizerTest, Init) {
  EXPECT_TRUE(quantizer_.enabled());
}

TEST_F(QuantizerTest, SingleNote) {

  uint16_t mask = 0x1;
  quantizer_.Configure(braids::scales[2], mask);

  EXPECT_EQ(0, quantizer_.Lookup(64));
  EXPECT_EQ(kOctave, quantizer_.Lookup(65));
  EXPECT_EQ(-kOctave, quantizer_.Lookup(63));

  EXPECT_EQ(kOctave, quantizer_.Process(kOctave));
  EXPECT_EQ(kOctave + kOctave, quantizer_.Process(kOctave + kOctave - 127));
  EXPECT_EQ(0, quantizer_.Process(kOctave / 2));
  EXPECT_EQ(-kOctave, quantizer_.Process(-kOctave));
  EXPECT_EQ(0, quantizer_.Process(-128));
  EXPECT_EQ(0, quantizer_.Process(-kOctave/2));
}

TEST_F(QuantizerTest,MappingModeActual) {

  braids::Scale scale = {kOctave, 2, {0, 256}};
  quantizer_.Configure(scale, 0xffff, braids::Quantizer::MAPPING_ACTUAL);

  // Brute force codebook check

  EXPECT_EQ(0, quantizer_.Process(0));
  EXPECT_EQ(256, quantizer_.Process(129));
  EXPECT_EQ(kOctave, quantizer_.Process(1000));
}

TEST_F(QuantizerTest,MappingModeEqual) {

  braids::Scale scale = {kOctave, 2, {0, 256}};
  quantizer_.Configure(scale, 0xfff, braids::Quantizer::MAPPING_EQUAL);

  EXPECT_EQ(0, quantizer_.Process(0));
  EXPECT_EQ(256, quantizer_.Process(kOctave/2));
  EXPECT_EQ(1536, quantizer_.Process(1201));
}

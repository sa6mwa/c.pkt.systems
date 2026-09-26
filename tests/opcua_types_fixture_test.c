#include <assert.h>
#include <cpkt/opcua_types.h>
#include <string.h>

int main(void) {
  cpkt_opcua_CpktFixtureOptional input, copy, decoded;
  cpkt_opcua_CpktFixtureUnion variant, variant_copy, variant_decoded;
  cpkt_opcua_CpktFixtureFlags flags = cpkt_opcua_CPKTFIXTUREFLAGS_TOP;
  cpkt_opcua_UInt64 words[2];
  cpkt_opcua_Int64 optional;
  cpkt_opcua_ByteString bytes;
  cpkt_opcua_CpktFixtureOptional_init(&input);
  optional.high32 = 0x80000000U;
  optional.low32 = 0;
  words[0].high32 = 0xffffffffU;
  words[0].low32 = 0xffffffffU;
  words[1].high32 = 0;
  words[1].low32 = 0;
  input.word = &optional;
  input.itemsSize = 2;
  input.items = words;
  assert(cpkt_opcua_CpktFixtureOptional_copy(&input, &copy) == 0);
  assert(copy.word != &optional && copy.word->high32 == optional.high32);
  assert(copy.items != words && copy.items[0].high32 == 0xffffffffU);
  assert(cpkt_opcua_CpktFixtureOptional_equal(&input, &copy));
  assert(cpkt_opcua_type_encode_binary(
             &copy, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_CPKTFIXTUREOPTIONAL),
             &bytes) == 0);
  assert(cpkt_opcua_type_decode_binary(
             &bytes, &decoded,
             cpkt_opcua_type_at(CPKT_OPCUA_TYPES_CPKTFIXTUREOPTIONAL)) == 0);
  assert(cpkt_opcua_CpktFixtureOptional_equal(&copy, &decoded));
  cpkt_opcua_CpktFixtureOptional_clear(&copy);
  cpkt_opcua_CpktFixtureOptional_clear(&decoded);
  cpkt_opcua_ByteString_clear(&bytes);
  input.word = NULL;
  input.items = CPKT_OPCUA_EMPTY_ARRAY_SENTINEL;
  input.itemsSize = 0;
  assert(cpkt_opcua_CpktFixtureOptional_copy(&input, &copy) == 0);
  assert(copy.word == NULL && copy.items == CPKT_OPCUA_EMPTY_ARRAY_SENTINEL);
  cpkt_opcua_CpktFixtureOptional_clear(&copy);
  cpkt_opcua_CpktFixtureUnion_init(&variant);
  variant.switchField = cpkt_opcua_CPKTFIXTUREUNIONSWITCH_WORD;
  variant.fields.word = words[0];
  assert(cpkt_opcua_CpktFixtureUnion_copy(&variant, &variant_copy) == 0);
  assert(cpkt_opcua_CpktFixtureUnion_equal(&variant, &variant_copy));
  cpkt_opcua_CpktFixtureUnion_clear(&variant_copy);
  variant.switchField = cpkt_opcua_CPKTFIXTUREUNIONSWITCH_CHILDREN;
  input.word = &optional;
  input.items = words;
  input.itemsSize = 2;
  variant.fields.children.childrenSize = 1;
  variant.fields.children.children = &input;
  assert(cpkt_opcua_CpktFixtureUnion_copy(&variant, &variant_copy) == 0);
  assert(variant_copy.fields.children.children != &input);
  assert(cpkt_opcua_type_encode_binary(
             &variant_copy,
             cpkt_opcua_type_at(CPKT_OPCUA_TYPES_CPKTFIXTUREUNION),
             &bytes) == 0);
  assert(cpkt_opcua_type_decode_binary(
             &bytes, &variant_decoded,
             cpkt_opcua_type_at(CPKT_OPCUA_TYPES_CPKTFIXTUREUNION)) == 0);
  assert(cpkt_opcua_CpktFixtureUnion_equal(&variant_copy, &variant_decoded));
  cpkt_opcua_CpktFixtureUnion_clear(&variant_copy);
  cpkt_opcua_CpktFixtureUnion_clear(&variant_decoded);
  cpkt_opcua_ByteString_clear(&bytes);
  variant.switchField = (cpkt_opcua_CpktFixtureUnionSwitch)99;
  assert(cpkt_opcua_CpktFixtureUnion_copy(&variant, &variant_copy) != 0);
  assert(variant_copy.switchField == cpkt_opcua_CPKTFIXTUREUNIONSWITCH_NONE);
  assert(flags.high32 == 0x80000000U && flags.low32 == 0);
  assert(cpkt_opcua_type_encode_binary(
             &flags, cpkt_opcua_type_at(CPKT_OPCUA_TYPES_CPKTFIXTUREFLAGS),
             &bytes) == 0);
  assert(bytes.length == 8 && bytes.data[7] == 0x80 && bytes.data[0] == 0);
  cpkt_opcua_ByteString_clear(&bytes);
  return 0;
}

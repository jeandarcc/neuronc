#include "neuronc/cli/ModuleCppManifest.h"

using namespace neuron;

TEST(ModuleCppManifestParsesExports) {
  const std::string content =
      "[module]\n"
      "name = \"Tensorflow\"\n"
      "abi = \"c\"\n"
      "\n"
      "[export.Version]\n"
      "symbol = \"npp_tf_version\"\n"
      "params = []\n"
      "return = \"string\"\n"
      "\n"
      "[export.Add]\n"
      "symbol = \"npp_tf_add\"\n"
      "params = [\"int\", \"int\"]\n"
      "return = \"int\"\n";

  ModuleCppManifest manifest;
  ModuleCppManifestParser parser;
  ASSERT_TRUE(parser.parseString(content, "<modulecpp_manifest>", &manifest));
  ASSERT_TRUE(parser.errors().empty());
  ASSERT_EQ(manifest.name, "Tensorflow");
  ASSERT_EQ(manifest.abi, "c");
  ASSERT_EQ(manifest.exports.size(), 2u);
  ASSERT_TRUE(manifest.exports.count("Version") == 1u);
  ASSERT_TRUE(manifest.exports.count("Add") == 1u);
  ASSERT_EQ(manifest.exports.at("Version").returnType, "string");
  ASSERT_EQ(manifest.exports.at("Add").parameterTypes.size(), 2u);
  return true;
}

TEST(ModuleCppManifestRejectsUnsupportedTypes) {
  const std::string content =
      "[module]\n"
      "name = \"Tensorflow\"\n"
      "\n"
      "[export.Bad]\n"
      "symbol = \"npp_tf_bad\"\n"
      "params = [\"Tensor<float>\"]\n"
      "return = \"void\"\n";

  ModuleCppManifest manifest;
  ModuleCppManifestParser parser;
  ASSERT_FALSE(parser.parseString(content, "<modulecpp_manifest>", &manifest));
  ASSERT_FALSE(parser.errors().empty());
  return true;
}

#include "TestFramework.h"

#include <string>

#include "firmware/Json.h"
#include "firmware/Sha256.h"

using bdmcs48::firmware::JsonParser;
using bdmcs48::firmware::JsonValue;
using bdmcs48::firmware::Sha256;

TEST(Sha256, VectorVacio) {
    CHECK_EQ(Sha256::hexOf("", 0),
             std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

TEST(Sha256, VectorAbc) {
    CHECK_EQ(Sha256::hexOf("abc", 3),
             std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

TEST(Sha256, VectorDeDosBloques) {
    const std::string text = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK_EQ(Sha256::hexOf(text),
             std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
}

TEST(Sha256, ActualizacionPorTrozosDaElMismoDigest) {
    Sha256 hash;
    hash.update("abcdbcdecdefdefgefghfghi");
    hash.update("ghijhijkijkljklmklmnlmnomnopnopq");
    CHECK_EQ(hash.hexDigest(),
             std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
}

TEST(Sha256, MatchesComparaCorrectamente) {
    const std::string digest = Sha256::hexOf("hola", 4);
    CHECK(Sha256::matches("hola", 4, digest));
    CHECK(!Sha256::matches("holb", 4, digest));
    CHECK(!Sha256::matches("hola", 4, "no-es-un-hash"));
}

TEST(Json, ParseaUnObjeto) {
    JsonValue root;
    std::string error;
    CHECK(JsonParser::parse(R"({"a": 1, "b": "texto", "c": true, "d": null})", root, error));
    CHECK(root.isObject());
    const JsonValue* a = root.find("a");
    CHECK(a != nullptr);
    CHECK(a->isNumber());
    CHECK_NEAR(a->asNumber(), 1.0, 1e-12);
    const JsonValue* b = root.find("b");
    CHECK(b != nullptr && b->isString());
    CHECK_EQ(b->asString(), std::string("texto"));
    const JsonValue* c = root.find("c");
    CHECK(c != nullptr && c->isBoolean());
    CHECK(c->asBoolean());
    const JsonValue* d = root.find("d");
    CHECK(d != nullptr && d->isNull());
    CHECK(root.find("no-existe") == nullptr);
}

TEST(Json, ParseaArraysYNumeros) {
    JsonValue root;
    std::string error;
    CHECK(JsonParser::parse(R"([1, 2.5, -3, 1e3, [4, 5]])", root, error));
    CHECK(root.isArray());
    CHECK_EQ(root.asArray().size(), std::size_t{5});
    CHECK_NEAR(root.asArray()[1].asNumber(), 2.5, 1e-12);
    CHECK_NEAR(root.asArray()[3].asNumber(), 1000.0, 1e-12);
    CHECK(root.asArray()[4].isArray());
    CHECK_EQ(root.asArray()[4].asArray().size(), std::size_t{2});
}

TEST(Json, RechazaClavesDuplicadas) {
    JsonValue root;
    std::string error;
    CHECK(!JsonParser::parse(R"({"a": 1, "a": 2})", root, error));
    CHECK(!error.empty());
}

TEST(Json, RechazaBasuraAlFinal) {
    JsonValue root;
    std::string error;
    CHECK(!JsonParser::parse(R"({"a": 1} basura)", root, error));
}

TEST(Json, RechazaCadenaSinCerrar) {
    JsonValue root;
    std::string error;
    CHECK(!JsonParser::parse(R"({"a": "sin cerrar})", root, error));
}

TEST(Json, RechazaCaracterDeControl) {
    JsonValue root;
    std::string error;
    CHECK(!JsonParser::parse("{\"a\": \"salto\ncrudo\"}", root, error));
}

TEST(Json, EscapaUnicode) {
    JsonValue root;
    std::string error;
    CHECK(JsonParser::parse(R"({"a": "A"})", root, error));
    const JsonValue* a = root.find("a");
    CHECK(a != nullptr);
    CHECK_EQ(a->asString(), std::string("A"));
}

TEST(Json, RespetaElLimiteDeProfundidad) {
    std::string deep;
    for (int i = 0; i < 200; ++i) {
        deep += "[";
    }
    for (int i = 0; i < 200; ++i) {
        deep += "]";
    }
    JsonValue root;
    std::string error;
    CHECK(!JsonParser::parse(deep, root, error));
}

#include "bind_math.hpp"

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "script_manager.hpp"

namespace lucus
{
    void bind_vec3();
    // void bind_quat();

    void ConstructVec3(glm::vec3* self)
    {
        new(self) glm::vec3(0.0f);
    }

    void ConstructVec3f(float x, float y, float z, glm::vec3* self)
    {
        new(self) glm::vec3(x, y, z);
    }

    void Vec3Add_Generic(asIScriptGeneric* gen)
    {
        const glm::vec3* a = (const glm::vec3*)gen->GetArgObject(0);
        const glm::vec3* b = (const glm::vec3*)gen->GetArgObject(1);

        glm::vec3 result = *a + *b;

        *(glm::vec3*)gen->GetAddressOfReturnLocation() = result;
    }

    void Vec3Mul_Generic(asIScriptGeneric* gen)
    {
        const glm::vec3* v = (const glm::vec3*)gen->GetArgObject(0);
        float s = gen->GetArgFloat(1);

        glm::vec3 result = (*v) * s;

        *(glm::vec3*)gen->GetAddressOfReturnLocation() = result;
    }

    void Vec3Length_Generic(asIScriptGeneric* gen)
    {
        const glm::vec3* v = static_cast<const glm::vec3*>(gen->GetArgObject(0));

        float result = glm::length(*v);

        gen->SetReturnFloat(result);
    }

    void Vec3Normalize_Generic(asIScriptGeneric* gen)
    {
        const glm::vec3* v = (const glm::vec3*)gen->GetArgObject(0);

        glm::vec3 result = glm::normalize(*v);

        *(glm::vec3*)gen->GetAddressOfReturnLocation() = result;
    }
}

void lucus::bind_math_module()
{
    bind_vec3();
}

void lucus::bind_vec3()
{
    asIScriptEngine* engine = script_manager::instance().get_engine();

    int r = 0;

    using namespace binds;

    r = engine->RegisterObjectType(vec3_class_name, sizeof(glm::vec3), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<glm::vec3>()); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        vec3_class_name, asBEHAVE_CONSTRUCT,
        "void f()",
        asFUNCTION(ConstructVec3),
        asCALL_CDECL_OBJLAST
    ); assert(r >= 0);

    r = engine->RegisterObjectBehaviour(
        vec3_class_name, asBEHAVE_CONSTRUCT,
        "void f(float, float, float)",
        asFUNCTION(ConstructVec3f),
        asCALL_CDECL_OBJLAST
    ); assert(r >= 0);

    r = engine->RegisterObjectProperty(vec3_class_name, "float x", asOFFSET(glm::vec3, x)); assert(r >= 0);
    r = engine->RegisterObjectProperty(vec3_class_name, "float y", asOFFSET(glm::vec3, y)); assert(r >= 0);
    r = engine->RegisterObjectProperty(vec3_class_name, "float z", asOFFSET(glm::vec3, z)); assert(r >= 0);

    std::string operator_add = std::string(vec3_class_name) + " opAdd(const " + vec3_class_name + " &in) const";
    r = engine->RegisterObjectMethod(
        vec3_class_name,
        operator_add.c_str(),
        asFUNCTION(Vec3Add_Generic),
        asCALL_GENERIC
    ); assert(r >= 0);

    std::string operator_mul = std::string(vec3_class_name) + " opMul(float) const";
    r = engine->RegisterObjectMethod(
        vec3_class_name,
        operator_mul.c_str(),
        asFUNCTION(Vec3Mul_Generic),
        asCALL_GENERIC
    ); assert(r >= 0);

    std::string length_func = std::string("float Length(const ") + vec3_class_name + " &in)";
    r = engine->RegisterGlobalFunction(
        length_func.c_str(),
        asFUNCTION(Vec3Length_Generic),
        asCALL_GENERIC
    ); assert(r >= 0);

    std::string normalize_func = std::string(vec3_class_name) + " Normalize(const " + vec3_class_name + " &in)";
    r = engine->RegisterGlobalFunction(
        normalize_func.c_str(),
        asFUNCTION(Vec3Normalize_Generic),
        asCALL_GENERIC
    ); assert(r >= 0);
}
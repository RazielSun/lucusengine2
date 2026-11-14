enum MaterialTextureSetup
{
    BINDFULL_TEXTURES,
    BINDLESS_TEXTURES,
    MIXED_TEXTURES
}

ResourceBindingMode materialTextureMode(MaterialTextureSetup setup, bool alternate)
{
    if (setup == BINDLESS_TEXTURES || (setup == MIXED_TEXTURES && alternate))
    {
        return ResourceBindingMode::BINDLESS;
    }
    return ResourceBindingMode::BINDFULL;
}

Material@ makePreviewMaterial(const string &in shader, Texture@ albedo = null)
{
    Material@ material = Material(shader);
    material.SetUseFrameUniformBuffer(true);
    material.SetUseObjectUniformBuffer(true);
    material.SetUseVertexIndexBuffers(true);
    material.SetMaterialParams(8.f, 0.f, 0.f, 0.f);
    if (albedo !is null)
    {
        material.SetTexture(albedo, 0);
    }
    return material;
}

void addPreviewCube(Scene@ scene, Mesh@ mesh, Material@ material, float x, float y)
{
    RenderObject@ object = scene.NewObject();
    object.SetMesh(mesh);
    object.SetMaterial(material);
    object.SetPosition(Vec3(x, y, 0.f));
    object.SetRotationEuler(Vec3(-15.f, 25.f, 0.f));
    object.SetScale(Vec3(1.2f, 1.2f, 1.2f));
}

void setupMaterialsScene(RenderMode renderMode, MaterialTextureSetup textureSetup)
{
    g_renderer.SetRenderMode(renderMode);
    g_window_manager.CreateWindow(1280, 720, "Lucus Engine 2 - Static Materials");

    Scene@ scene = Scene();
    Camera@ camera = Camera();
    camera.SetPosition(Vec3(0.f, 0.f, 7.f));
    camera.SetRotationEuler(Vec3(0.f, 0.f, 0.f));
    scene.SetCamera(camera);

    DirectionalLight@ light = DirectionalLight();
    light.SetPosition(Vec3(-4.f, 6.f, 10.f));
    light.SetRotationEuler(Vec3(-25.f, -30.f, 0.f));
    scene.SetDirectionalLight(light);

    Mesh@ cube = Mesh::load("content/assets/geometry/material_cube.gltf");

    Texture@ checkerA = Texture::load("content/assets/textures/uv_checker_a.jpg");
    checkerA.SetResourceBindingMode(materialTextureMode(textureSetup, false));
    Texture@ checkerB = Texture::load("content/assets/textures/uv_checker_b.jpg");
    checkerB.SetResourceBindingMode(materialTextureMode(textureSetup, true));
    Texture@ photograph = Texture::load("content/assets/textures/texture.jpg");
    photograph.SetResourceBindingMode(materialTextureMode(textureSetup, false));

    Material@ red = makePreviewMaterial("simple_unlit");
    red.SetBaseColor(1.f, 0.15f, 0.15f);
    Material@ green = makePreviewMaterial("simple_unlit");
    green.SetBaseColor(0.15f, 1.f, 0.15f);
    Material@ blue = makePreviewMaterial("simple_unlit");
    blue.SetBaseColor(0.15f, 0.15f, 1.f);
    Material@ yellow = makePreviewMaterial("simple_unlit");
    yellow.SetBaseColor(1.f, 0.8f, 0.15f);

    addPreviewCube(scene, cube, red, -3.15f, 2.1f);
    addPreviewCube(scene, cube, green, -1.05f, 2.1f);
    addPreviewCube(scene, cube, blue, 1.05f, 2.1f);
    addPreviewCube(scene, cube, yellow, 3.15f, 2.1f);

    Material@ coarseChecker = makePreviewMaterial("checker_unlit");
    coarseChecker.SetMaterialParams(4.f, 0.f, 0.f, 0.f);
    Material@ fineChecker = makePreviewMaterial("checker_unlit");
    fineChecker.SetMaterialParams(16.f, 0.f, 0.f, 0.f);
    addPreviewCube(scene, cube, coarseChecker, -3.15f, 0.f);
    addPreviewCube(scene, cube, fineChecker, -1.05f, 0.f);
    addPreviewCube(scene, cube, makePreviewMaterial("simple_texture_unlit", checkerA), 1.05f, 0.f);
    addPreviewCube(scene, cube, makePreviewMaterial("simple_texture_unlit", checkerB), 3.15f, 0.f);

    Material@ litColor = makePreviewMaterial("simple_lit");
    litColor.SetBaseColor(0.75f, 0.85f, 1.f);
    addPreviewCube(scene, cube, litColor, -3.15f, -2.1f);
    addPreviewCube(scene, cube, makePreviewMaterial("simple_texture_lit", photograph), -1.05f, -2.1f);
    addPreviewCube(scene, cube, makePreviewMaterial("simple_texture_unlit"), 1.05f, -2.1f);
    addPreviewCube(scene, cube, makePreviewMaterial("simple_texture_lit", checkerA), 3.15f, -2.1f);

    g_renderer.SetCurrentScene(scene);
}

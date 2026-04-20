void main()
{
    g_window_manager.CreateWindow(1280, 720, "Lucus Engine 2 - One Texture Test");

    Scene@ scene = Scene();
    g_renderer.SetCurrentScene(scene);

    Camera@ camera = Camera();
    camera.SetPosition(Vec3(0.f, 0.f, 5.f));
    scene.SetCamera(camera);

    Mesh@ cube = Mesh::load("content/assets/geometry/cube.gltf");

    Material@ cube_mat = Material("simple_texture_unlit");
    cube_mat.SetUseFrameUniformBuffer(true);
    cube_mat.SetUseObjectUniformBuffer(true);
    cube_mat.SetUseVertexIndexBuffers(true);

    Texture@ tex = Texture::load("content/assets/textures/texture.jpg");
    cube_mat.SetTexture(tex, 0);

    RenderObject@ cube_obj2 = scene.NewObject();
    cube_obj2.SetPosition(Vec3(0.f, 0.f, 0.f));
    cube_obj2.SetRotationEuler(Vec3(20.f, -30.f, 15.f));
    cube_obj2.SetScale(Vec3(2.f, 2.f, 2.f));
    cube_obj2.SetMesh(cube);
    cube_obj2.SetMaterial(cube_mat);
}
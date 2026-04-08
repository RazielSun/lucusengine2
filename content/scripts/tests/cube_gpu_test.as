void main()
{
    g_window_manager.CreateWindow(1280, 720, "Lucus Engine 2 - Cube GPU Test");
    
    Scene@ scene = Scene();
    g_renderer.SetCurrentScene(scene);

    Camera@ camera = Camera();
    camera.SetPosition(Vec3(0.f, 0.f, 5.f));
    scene.SetCamera(camera);

    Mesh@ cube = Mesh("", 36); // gpu hardcoded cube

    Material@ cube_mat = Material("cube"); // gpu hardcoded cube
    cube_mat.SetUseUniformBuffers(true);

    RenderObject@ cube_obj1 = scene.NewObject();
    cube_obj1.SetPosition(Vec3(-0.8f, 0.2f, 0.f));
    cube_obj1.SetRotationEuler(Vec3(0.f, 0.f, 0.f));
    cube_obj1.SetMesh(cube);
    cube_obj1.SetMaterial(cube_mat);

    RenderObject@ cube_obj2 = scene.NewObject();
    cube_obj2.SetPosition(Vec3(0.f, 0.f, 0.f));
    cube_obj2.SetRotationEuler(Vec3(0.f, 10.f, 45.f));
    cube_obj2.SetMesh(cube);
    cube_obj2.SetMaterial(cube_mat);

    RenderObject@ cube_obj3 = scene.NewObject();
    cube_obj3.SetPosition(Vec3(0.8f, 0.2f, 0.f));
    cube_obj3.SetRotationEuler(Vec3(90.f, 0.f, 0.f));
    cube_obj3.SetMesh(cube);
    cube_obj3.SetMaterial(cube_mat);
}
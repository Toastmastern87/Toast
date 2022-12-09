using Toast;

class ClientHelloWorld : EntityOld
{
    public float Speed = 1.0f;

    private TagComponent mTag;
    private TransformComponent mTransform;
    private PlanetComponent mPlanet;

    void OnCreate()
    {
        mTag = GetComponent<TagComponent>();
        mTransform = GetComponent<TransformComponent>();
        mPlanet = GetComponent<PlanetComponent>();

        Toast.Console.LogInfo("Creating Entity: " + mTag.Tag);
    }

    void OnUpdate(float ts)
    {
        mTransform.Translation = mTransform.Translation + new Vector3(ts * Speed, 0.0f, 0.0f);
        if (Input.IsKeyPressed(KeyCode.A)) 
            Toast.Console.LogInfo("A Pressed");

        if(Input.GetMouseWheelDelta() != 0.0f)
            Toast.Console.LogInfo("Mouse wheel delta: " + Input.GetMouseWheelDelta());
        Input.SetMouseWheelDelta(0.0f);

        Toast.Console.LogWarning("Planet Radius: " + mPlanet.Radius);

    }
}

using Toast;

class ClientHelloWorld : Entity
{
    private TagComponent mTag;
    private TransformComponent mTransform;

    void OnCreate()
    {
        mTag = GetComponent<TagComponent>();
        mTransform = GetComponent<TransformComponent>();

        Toast.Console.LogInfo("Creating Entity: " + mTag.Tag);
    }

    void OnUpdate(float ts)
    {
        mTransform.Translation = mTransform.Translation + new Vector3(ts, 0.0f, 0.0f);
    }
}

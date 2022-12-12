using Toast;

namespace Sandbox
{
    public class Player : Entity
    {
        private TransformComponent mTransform;

        void OnCreate() 
        {
            System.Console.WriteLine($"Player.OnCreated - {ID}");

            mTransform = GetComponent<TransformComponent>();

            bool hasTransform = HasComponent<TransformComponent>();

            Console.LogTrace("Has Transform: " + hasTransform);

            //mTransform.Translation = new Vector3(0.0f);
        }

        void OnUpdate(float ts)
        {
            System.Console.WriteLine($"Player.OnUpdate: {ts}");
        }
    }
}

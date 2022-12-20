using Toast;

namespace Sandbox
{
    public class Player : Entity
    {
        private TransformComponent mTransform;

        public float Speed;
        public Entity OtherPlayer;

        void OnCreate() 
        {
            System.Console.WriteLine($"Player.OnCreated - {ID}");

            mTransform = GetComponent<TransformComponent>();

            bool hasTransform = HasComponent<TransformComponent>();

            Console.LogTrace("Has Transform: " + hasTransform);

            //Speed = 0.25f;

            //mTransform.Translation = new Vector3(0.0f);
        }

        void OnUpdate(float ts)
        {
            System.Console.WriteLine($"Player.OnUpdate: {ts}");
        }
    }
}

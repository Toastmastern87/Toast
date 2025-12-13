using System;
using System.IO;
using Toast;

namespace Sandbox
{
    public class StarshipController : Entity
    {
        private Entity mStarship;
        private Entity mRB1;
        private Entity mRB2;
        private Entity mRB3;
        private RigidBodyComponent mRigidBody;
        private BoxColliderComponent mBoxCollider;

        private ParticlesComponent mRB1Particles;
        private ParticlesComponent mRB2Particles;
        private ParticlesComponent mRB3Particles;

        private bool mShutdownEngines = false;
        private bool mEnginesRunning = false;

        void OnCreate()
        {
            mStarship = FindEntityByName("Starship2"); 
            mRB1 = FindChildEntityByName("Starship2", "Rocket Exhaust RB1");
            mRB2 = FindChildEntityByName("Starship2", "Rocket Exhaust RB2");
            mRB3 = FindChildEntityByName("Starship2", "Rocket Exhaust RB3");
            mRigidBody = mStarship.GetComponent<RigidBodyComponent>();
            mBoxCollider = mStarship.GetComponent<BoxColliderComponent>();

            mRB1Particles = mRB1.GetComponent<ParticlesComponent>();
            mRB2Particles = mRB2.GetComponent<ParticlesComponent>();
            mRB3Particles = mRB3.GetComponent<ParticlesComponent>();
        }

        void OnEvent()
        {
        }

        void OnUpdate(float ts)
        {
            float altitude = mBoxCollider.Altitude;

            if (altitude <= 90.0f && altitude > 0.5f && !mShutdownEngines) 
            {
                mRB1Particles.Emitting = true;
                mRB2Particles.Emitting = true;
                mRB3Particles.Emitting = true;

                mEnginesRunning = true;
            }
            else if(altitude <= 0.5)
            {
                mRB1Particles.Emitting = false;
                mRB2Particles.Emitting = false;
                mRB3Particles.Emitting = false;

                mShutdownEngines = true;
                mEnginesRunning = false;
            }

            if (mEnginesRunning) 
            {
                Vector3 thrustForce = new Vector3(0.0f, 39272.0f, 0.0f); // Newtons
                Vector3 impulse = thrustForce * ts;       // kg·m/s
                PhysicsEngine.ApplyLinearImpulse(this.ID, impulse);
            }

        }
    }
}

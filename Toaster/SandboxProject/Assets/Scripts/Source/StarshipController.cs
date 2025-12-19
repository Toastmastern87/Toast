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
            mStarship = this; 
            mRB1 = FindChildEntityByName(this.Name, "Rocket Exhaust RB1");
            mRB2 = FindChildEntityByName(this.Name, "Rocket Exhaust RB2");
            mRB3 = FindChildEntityByName(this.Name, "Rocket Exhaust RB3");
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

            Vector3 v = mRigidBody.LinearVelocity;

            const float groundAltEps = 0.5f;          
            const float stopSpeedEps = 0.25f;

            if (altitude <= 90.0f && altitude > groundAltEps && !mShutdownEngines)
                mEnginesRunning = true;

            if (mEnginesRunning && altitude <= groundAltEps && Vector3.Length(v) <= stopSpeedEps)
            {
                mEnginesRunning = false;
                mShutdownEngines = true;
            }

            mRB1Particles.Emitting = mEnginesRunning;
            mRB2Particles.Emitting = mEnginesRunning;
            mRB3Particles.Emitting = mEnginesRunning;

            if (mEnginesRunning)
            {
                Vector3 thrustForce = new Vector3(0.0f, 38413.0f, 0.0f); // N (updated for your new start altitude)
                Vector3 impulse = thrustForce * ts;             // kg·m/s
                PhysicsEngine.ApplyLinearImpulse(this.ID, impulse);
            }
        }
    }
}

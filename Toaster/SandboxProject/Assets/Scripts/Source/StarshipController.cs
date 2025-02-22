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

        private ParticlesComponent mRB1Particles;
        private ParticlesComponent mRB2Particles;
        private ParticlesComponent mRB3Particles;

        void OnCreate()
        {
            mStarship = FindEntityByName("Starship SN3"); 
            mRB1 = FindChildEntityByName("Starship SN3", "Rocket Exhaust RB1");
            mRB2 = FindChildEntityByName("Starship SN3", "Rocket Exhaust RB2");
            mRB3 = FindChildEntityByName("Starship SN3", "Rocket Exhaust RB3");
            mRigidBody = mStarship.GetComponent<RigidBodyComponent>();

            mRB1Particles = mRB1.GetComponent<ParticlesComponent>();
            mRB2Particles = mRB2.GetComponent<ParticlesComponent>();
            mRB3Particles = mRB3.GetComponent<ParticlesComponent>();

            mRigidBody.ReqAltitude = true;
        }

        void OnEvent()
        {
        }

        void OnUpdate(float ts)
        {
            if (mRigidBody.Altitude <= 130.0f && mRigidBody.Altitude > 1.0) 
            {
                mRB1Particles.Emitting = true;
                mRB2Particles.Emitting = true;
                mRB3Particles.Emitting = true;
            }
        }
    }
}

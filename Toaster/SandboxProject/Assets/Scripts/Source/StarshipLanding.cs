using Source.Toast.Math;
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

using Toast;

namespace Sandbox
{
    public class StarshipLanding : Entity
    {
        private enum LandingState
        {
            ReentryInterface,   // Future: transitioning from orbital speed to subsonic regime
            BellyFlopFreeFall,  // Belly-flop descent, engines off, monitoring for flip trigger
            FlipPrepare,        // Gimbal engines to flip angle before ignition
            FlipInitiate,       // Engines on, gimbal offset, rotating toward vertical
            FlipArrest,         // PD controller arresting rotation near vertical
            LandingBurn,        // Vertical, decelerating to touchdown
            Landed              // Touchdown — engines off
        }

        public float MinTriggerSpeed = 10.0f;
        public float SafetyMarginMeters = 300.0f;    // Extra altitude buffer above computed minimum

        public float FlipGimbalAngleDeg = -15.0f;   // Gimbal to initiate flip (negative = toward heat shield)
        public float GimbalSlewRate = 15.0f;     // Degrees per second the gimbal can move
        public float FlipThrottle = 0.5f;   // Throttle during flip (lower = less speed gain while flipping)
        public float ArrestThrottle = 0.1f;
        public float MinArrestThrottle = 0.1f;

        public float LandingThrottle = 1.0f;      // Throttle during final descent
        public float TargetTouchdownSpeed = 2.0f;      // m/s at ground contact
        public float ThrottleDownAltitude = 50.0f;     // Below this altitude, reduce throttle for soft landing

        public float MaxThrustNewtons = 2200000.0f; // Single Raptor-class engine
        public int ActiveEngineCount = 3;          // Engines used for landing
        public float PitchVerticalThresh = 8.0f; // Degrees from vertical to transition out of flip

        public float ArrestKp = 0.4f;      // Proportional gain: gimbal per degree of pitch error
        public float ArrestKd = 0.8f;   // Derivative gain: gimbal per deg/s of angular velocity
        public float MaxArrestGimbal = 15.0f;     // Max gimbal magnitude during arrest (degrees)
        public float AngVelDeadband = 2.0f;      // deg/s — "rotation stopped" threshold

        private TransformComponent mShipTransform;
        private RigidBodyComponent mRigidBody;
        private BoxColliderComponent mBoxCollider;

        private Entity mStarship;
        private Entity mRS1;
        private Entity mRS2;
        private Entity mRS3;

        private TransformComponent mRaptorSea1TC;
        private TransformComponent mRaptorSea2TC;
        private TransformComponent mRaptorSea3TC;

        private ParticlesComponent mRS1Particles;
        private ParticlesComponent mRS2Particles;
        private ParticlesComponent mRS3Particles;

        private LandingState landingState = LandingState.BellyFlopFreeFall;
        private float flipDurationEstimate = 3.5f;     // Estimated seconds for a full 90° flip (altitude budget)
        private float flipStartTime = 0f;
        private float GimbalTrackingRate = 90.0f;
        private float elapsedTime = 0f;
        private Vector3 flipAxis;
        private float lastPitch = 180f;
        private float smoothedAngVel = 0f;

        // --- Engine control state ---
        private float currentThrottle = 0f;
        private bool enginesActive = false;

        void OnCreate()
        {
            mStarship = this;
            mRS1 = FindChildEntityByName(this.Name, "RaptorSea1");
            mRS2 = FindChildEntityByName(this.Name, "RaptorSea2");
            mRS3 = FindChildEntityByName(this.Name, "RaptorSea3");

            mRigidBody = this.GetComponent<RigidBodyComponent>();
            mShipTransform = this.GetComponent<TransformComponent>();
            mBoxCollider = this.GetComponent<BoxColliderComponent>();

            mRaptorSea1TC = mRS1.GetComponent<TransformComponent>();
            mRaptorSea2TC = mRS2.GetComponent<TransformComponent>();
            mRaptorSea3TC = mRS3.GetComponent<TransformComponent>();

            mRS1Particles = mRS1.GetComponent<ParticlesComponent>();
            mRS2Particles = mRS2.GetComponent<ParticlesComponent>();
            mRS3Particles = mRS3.GetComponent<ParticlesComponent>();
        }

        void OnEvent()
        {

        }

        void OnUpdate(float ts)
        {
            elapsedTime += ts;

            float altitude = mBoxCollider.Altitude;
            float radialVel = GetRadialVelocity();
            float descentSpeed = Math.Max(0.0f, -radialVel);
            float pitch = GetPitchFromVertical();
            float angVel = GetAngularVelocityPitch(pitch, ts);
            float gravity = (float)Planet.Gravity;

            switch (landingState)
            {
                case LandingState.BellyFlopFreeFall:
                    UpdateBellyFreeFall(altitude, descentSpeed, pitch, gravity);
                    break;
                case LandingState.FlipPrepare:
                    UpdateFlipPrepare(altitude, descentSpeed, pitch, gravity, ts);
                    break;
                case LandingState.FlipInitiate:
                    UpdateFlipInitiate(altitude, descentSpeed, pitch, angVel, gravity, ts);
                    break;
                case LandingState.FlipArrest:
                    UpdateFlipArrest(altitude, descentSpeed, pitch, angVel, gravity);
                    break;
                case LandingState.LandingBurn:
                    UpdateLandingBurn(altitude, descentSpeed, pitch, angVel, gravity);
                    break;
            }

            // Apply thrust from all active engines every frame
            ApplyThrust(ts);
        }

        private void UpdateBellyFreeFall(float altitude, float descentSpeed, float pitch, float gravity)
        {
            // Don't trigger if we're barely moving (e.g. just released from drop) THIS CAN BE REMOVED LATER
            if (descentSpeed < MinTriggerSpeed)
                return;

            float triggerAlt = ComputeTriggerAltitude(descentSpeed, gravity);

            // Debug: you might want to display triggerAlt in your HUD
            //Toast.Console.LogInfo($"[BellyFlopFreeFall] alt={altitude:F0} trigger={triggerAlt:F0} vDown={descentSpeed:F1}");

            if (altitude <= triggerAlt)
            {
                // Capture the ship's local Z axis in world space — this is the flip axis
                Vector3 forward = mShipTransform.WorldRight;
                flipAxis = Vector3.Normalize(forward);

                //Toast.Console.LogInfo($"[Landing] flipAxis=({flipAxis.X:F2}, {flipAxis.Y:F2}, {flipAxis.Z:F2})");

                //Toast.Console.LogInfo($"[Landing] FLIP TRIGGER at alt={altitude:F0}m, speed={descentSpeed:F1}m/s, budget={triggerAlt:F0}m");
                TransitionTo(LandingState.FlipPrepare);
            }
        }

        private void UpdateFlipPrepare(float altitude, float descentSpeed, float pitch, float gravity, float dt)
        {
            SetGimbalAngle(FlipGimbalAngleDeg);

            // Check if gimbal is in position
            bool gimbalReady = mRaptorSea1TC.HasReachedTargetRotation(0.1f) && mRaptorSea2TC.HasReachedTargetRotation(0.1f) && mRaptorSea3TC.HasReachedTargetRotation(0.1f);

            if (gimbalReady)
            {
                //Toast.Console.LogInfo($"[Landing] ENGINES IGNITION at alt={altitude:F0}m");

                flipStartTime = GetTime();

                TransitionTo(LandingState.FlipInitiate);

                SetEngineActive(true);
            }
        }

        private void UpdateFlipInitiate(float altitude, float descentSpeed, float pitch, float angVel, float gravity, float dt)
        {
            // Transition to arrest phase when we're getting close to vertical
            // Use a larger transition zone so the PD controller has room to work
            float arrestTransitionAngle = PitchVerticalThresh;  // e.g., 40° from vertical

            if (Math.Abs(pitch) < arrestTransitionAngle)
            {
                //Toast.Console.LogInfo($"[Landing] FLIP ARREST at pitch={pitch:F1}°, angVel={angVel:F1}°/s");
                TransitionTo(LandingState.FlipArrest);
                return;
            }

            // Safety: if flip is taking way too long, something is wrong
            float elapsed = GetTime() - flipStartTime;
            if (elapsed > flipDurationEstimate * 3.0f)
            {
                //Toast.Console.LogInfo("[Landing] WARNING: Flip taking too long, forcing arrest phase");
                TransitionTo(LandingState.FlipArrest);
                return;
            }

            // Keep gimbal at flip angle — the torque is doing the work
            SetGimbalAngle(FlipGimbalAngleDeg);
            SetThrottle(FlipThrottle);
        }

        private void UpdateFlipArrest(float altitude, float descentSpeed, float pitch, float angVel, float gravity)
        {
            float gimbalCommand = ArrestKp * pitch + ArrestKd * angVel;
            gimbalCommand = Clamp(gimbalCommand, -MaxArrestGimbal, MaxArrestGimbal);

            // Throttle proportional to how much rotation remains
            // High angVel or large pitch = high throttle for authority
            // Near vertical and slow = minimal throttle to preserve descent speed
            float pitchNeed = Math.Abs(pitch) / 90.0f;          // 0 at vertical, 1 at horizontal
            float angVelNeed = Math.Abs(angVel) / 100.0f;       // 0 when stopped, 1 at 100°/s
            float need = Math.Max(pitchNeed, angVelNeed);        // Whichever is more urgent
            need = Clamp(need, 0.0f, 1.0f);

            float throttle = MinArrestThrottle + need * (ArrestThrottle - MinArrestThrottle);

            SetGimbalAngle(gimbalCommand);
            SetThrottle(throttle);

            //Toast.Console.LogInfo($"[Arrest] pitch={pitch:F1} angVel={angVel:F1} gimbal={gimbalCommand:F1} throttle={throttle:F2}");

            bool nearVertical = Math.Abs(pitch) < 6.0f;
            bool rotationStopped = Math.Abs(angVel) < AngVelDeadband;

            if (nearVertical && rotationStopped)
            {
                //Toast.Console.LogInfo($"[Landing] VERTICAL at alt={altitude:F0}m");

                TransitionTo(LandingState.LandingBurn);
            }
        }

        private void UpdateLandingBurn(float altitude, float descentSpeed, float pitch, float angVel, float gravity)
        {
            // --- Attitude hold + lateral velocity correction ---
            // Base PD controller to hold vertical
            float gimbalCommand = ArrestKp * pitch + ArrestKd * angVel;

            // Steer into lateral velocity to cancel horizontal drift
            // Get velocity component in the flip plane (perpendicular to radial up and flip axis)
            Vector3 velocity = mRigidBody.LinearVelocity;
            Vector3 radialUp = GetRadialUp();

            // Lateral direction in the flip plane: cross(flipAxis, radialUp)
            Vector3 lateralDir = new Vector3(
                flipAxis.Y * radialUp.Z - flipAxis.Z * radialUp.Y,
                flipAxis.Z * radialUp.X - flipAxis.X * radialUp.Z,
                flipAxis.X * radialUp.Y - flipAxis.Y * radialUp.X
            );

            // Project velocity onto this direction
            float lateralSpeed = velocity.X * lateralDir.X + velocity.Y * lateralDir.Y + velocity.Z * lateralDir.Z;

            // Add correction: tilt into the drift to cancel it
            float lateralKp = 0.4f;  // Tune this — degrees of gimbal per m/s of lateral speed
            gimbalCommand += lateralKp * lateralSpeed;

            gimbalCommand = Clamp(gimbalCommand, -MaxArrestGimbal * 0.5f, MaxArrestGimbal * 0.5f);
            SetGimbalAngle(gimbalCommand);

            // --- Throttle: constant-deceleration profile ---
            // We want to arrive at altitude=0 with TargetTouchdownSpeed
            // Required deceleration: a = (v² - vf²) / (2 * altitude)
            // Required thrust: F = m * (a + g)
            // Throttle = F / F_max

            float totalThrust = MaxThrustNewtons * ActiveEngineCount;

            if (altitude > 0.5f && descentSpeed > TargetTouchdownSpeed)
            {
                float desiredDecel = (descentSpeed * descentSpeed - TargetTouchdownSpeed * TargetTouchdownSpeed)
                                      / (2.0f * altitude);
                float requiredAccel = desiredDecel + gravity;
                float throttle = (requiredAccel * mRigidBody.Mass) / totalThrust;

                // Near ground, blend to a gentle settling profile
                if (altitude < ThrottleDownAltitude)
                {
                    float gentleThrottle = (gravity + 0.5f) * mRigidBody.Mass / totalThrust;
                    float blend = altitude / ThrottleDownAltitude;
                    throttle = Lerp(gentleThrottle, throttle, blend);
                }

                throttle = Clamp(throttle, 0.05f, 1.0f);
                SetThrottle(throttle);
            }
            else
            {
                // Slow enough — gentle settle, slightly below hover thrust
                float hoverThrottle = gravity * mRigidBody.Mass / totalThrust;
                SetThrottle(hoverThrottle * 0.95f);
            }

            //Toast.Console.LogInfo($"[LandingBurn] alt={altitude:F0} speed={descentSpeed:F1} pitch={pitch:F1} throttle={currentThrottle:F2}");

            // Detect touchdown
            if (altitude < 0.5f && descentSpeed < TargetTouchdownSpeed + 1.0f)
            {
                Toast.Console.LogInfo($"[Landing] touchdown at speed={descentSpeed:F2}m/s, pitch={pitch:F1}°");
                SetEngineActive(false);
                SetThrottle(0f);
                SetGimbalAngle(0f);
                TransitionTo(LandingState.Landed);
            }
        }

        private void TransitionTo(LandingState newState)
        {
            Toast.Console.LogInfo($"[Landing] Transitioning from {landingState} to {newState}");

            landingState = newState;
        }

        private static float Clamp(float value, float min, float max)
        {
            return value < min ? min : (value > max ? max : value);
        }

        private static float Lerp(float a, float b, float t)
        {
            return a + (b - a) * Clamp(t, 0f, 1f);
        }

        private float ComputeTriggerAltitude(float descentSpeed, float gravity)
        {
            float gimbalSlewTime = Math.Abs(FlipGimbalAngleDeg) / GimbalSlewRate;
            float prepAltitude = descentSpeed * gimbalSlewTime;

            float flipAltitude = descentSpeed * flipDurationEstimate;

            // Estimate speed lost during arrest phase
            float totalThrust = MaxThrustNewtons * ActiveEngineCount;
            float arrestAccel = totalThrust * ArrestThrottle / mRigidBody.Mass - gravity;
            float arrestTime = 3.0f;  // Rough estimate — tune from your logs
            float speedLostInArrest = arrestAccel * arrestTime;
            float speedAfterArrest = Math.Max(descentSpeed - speedLostInArrest, 10.0f);

            // Landing burn from remaining speed
            float thrustAccel = totalThrust * LandingThrottle / mRigidBody.Mass;
            float netDecel = thrustAccel - gravity;

            if (netDecel <= 0.1f)
            {
                Toast.Console.LogInfo("[Landing] WARNING: Insufficient thrust for landing!");
                return 99999f;
            }

            float landingBurnAlt = (speedAfterArrest * speedAfterArrest - TargetTouchdownSpeed * TargetTouchdownSpeed)
                                    / (2.0f * netDecel);

            return prepAltitude + flipAltitude + landingBurnAlt + SafetyMarginMeters;
        }

        private Vector3 GetRadialUp()
        {
            // [API] You need access to the ship's world position here.
            //       Replace with however your scripting API exposes entity translation.
            //       e.g. Entity.Transform.Translation, or an InternalCalls getter.
            Vector3 shipPos = mShipTransform.Translation;  // [API] adjust to your accessor
            Vector3 planetCenter = Planet.Translation;

            Vector3 radial = new Vector3(shipPos.X - planetCenter.X, shipPos.Y - planetCenter.Y, shipPos.Z - planetCenter.Z);

            // Normalize (with safety for zero-distance edge case)
            float length = (float)Math.Sqrt(radial.X * radial.X + radial.Y * radial.Y + radial.Z * radial.Z);
            if (length < 0.001f)
                return new Vector3(0.0f, 1.0f, 0.0f); // Fallback

            float inv = 1.0f / length;
            return new Vector3(radial.X * inv, radial.Y * inv, radial.Z * inv);
        }

        private float GetRadialVelocity()
        {
            Vector3 velocity = mRigidBody.LinearVelocity; 
            Vector3 radialUp = GetRadialUp();

            // Dot product: project velocity onto the radial direction
            return velocity.X * radialUp.X + velocity.Y * radialUp.Y + velocity.Z * radialUp.Z;
        }

        private float GetPitchFromVertical()
        {
            Vector3 shipUp = mShipTransform.WorldUp;
            Vector3 radialUp = GetRadialUp();

            // Project the angle onto the flip plane using atan2
            // This gives a smooth, continuous signed angle with no sign detection needed
            float dot = shipUp.X * radialUp.X + shipUp.Y * radialUp.Y + shipUp.Z * radialUp.Z;

            // cross(radialUp, shipUp) projected onto the stored flip axis = sin of the angle
            Vector3 cross = new Vector3(
                radialUp.Y * shipUp.Z - radialUp.Z * shipUp.Y,
                radialUp.Z * shipUp.X - radialUp.X * shipUp.Z,
                radialUp.X * shipUp.Y - radialUp.Y * shipUp.X
            );
            float sinAngle = cross.X * flipAxis.X + cross.Y * flipAxis.Y + cross.Z * flipAxis.Z;

            return (float)Math.Atan2(sinAngle, dot) * (180.0f / (float)Math.PI);
        }

        private void SetEngineActive(bool active)
        {
            enginesActive = active;
            mRS1Particles.Emitting = active;
            mRS2Particles.Emitting = active;
            mRS3Particles.Emitting = active;
        }

        private void SetGimbalAngle(float degrees, float slewRate = -1f)
        {
            if (slewRate < 0f)
                slewRate = GimbalTrackingRate;  // Default to fast tracking

            mRaptorSea1TC.AngularSpeed = slewRate;
            mRaptorSea1TC.SetTargetRotation(degrees, 0.0f, 0.0f);
            mRaptorSea2TC.AngularSpeed = slewRate;
            mRaptorSea2TC.SetTargetRotation(degrees, 0.0f, 0.0f);
            mRaptorSea3TC.AngularSpeed = slewRate;
            mRaptorSea3TC.SetTargetRotation(degrees, 0.0f, 0.0f);
        }

        private void SetThrottle(float throttle01)
        {
            currentThrottle = Clamp(throttle01, 0f, 1f);
        }

        private void ApplyThrust(float dt)
        {
            if (!enginesActive || currentThrottle <= 0f)
                return;

            // Total thrust from all engines, applied at a single center point
            float totalThrust = MaxThrustNewtons * ActiveEngineCount * currentThrottle;

            // Thrust direction from the ship's rotation + gimbal
            // Use engine 1's gimbal angle (all three are identical)
            Quaternion shipRot = mShipTransform.Rotation;
            Quaternion engineLocalRot = mRaptorSea1TC.Rotation;
            Quaternion engineWorldRot = Quaternion.Multiply(shipRot, engineLocalRot);
            Vector3 thrustDir = Vector3.Normalize(Vector3.Rotate(engineWorldRot, new Vector3(0.0f, 1.0f, 0.0f)));

            //Toast.Console.LogCritical("Thrust Direction: " + thrustDir.ToString());

            // Application point: center of the three engines in local space,
            // then transformed to world space
            Vector3 centerLocal = new Vector3(0.0f, mRaptorSea1TC.Translation.Y, 0.0f);
            Vector3 centerWorld = mShipTransform.Translation + Vector3.Rotate(shipRot, centerLocal);

            Vector3 impulse = thrustDir * totalThrust * dt;
            PhysicsEngine.ApplyLinearImpulseAtPoint(this.ID, impulse, centerWorld);
        }

        private float GetAngularVelocityPitch(float currentPitch, float dt)
        {
            if (dt <= 0.0001f)
                return smoothedAngVel;

            float raw = (currentPitch - lastPitch) / dt;
            lastPitch = currentPitch;

            smoothedAngVel = smoothedAngVel * 0.7f + raw * 0.3f;
            return smoothedAngVel;
        }

        private float GetTime()
        {
            return elapsedTime;
        }
    }
}
using Sandbox.Source;
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

        public float LegDeployAltitude = 500.0f;

        // Engine ramp variables
        public float IgnitionRampTime = 0.5f;       // seconds from ignition to full plume
        public float IgnitionLifeTimeStart = 0.01f; // near-zero: plume starts at the nozzle
        public float IgnitionIntensityStart = 0.5f; // dim glow before chamber pressure builds
        public float IgnitionSpawnDelayMult = 4.0f; // 4x delay = 1/4 the particle density

        // Authored "full throttle" values, captured once so the ramp has a target.
        private float mFullLifeTime;
        private float mFullSpawnDelay;
        private float mFullStartIntensity;
        private float mIgnitionTime = -1f;          // -1 = engines have never fired

        private TransformComponent mShipTransform;
        private RigidBodyComponent mRigidBody;
        private BoxColliderComponent mBoxCollider;
        private MeshComponent mMesh;

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

        private Entity mStarshipInfoPanel;
        private Entity mVelocityEntity;
        private Entity mAltitudeEntity;
        private Entity mUnloadCargoButton;
        private Entity mLoadCargoButton;
        private UIPanelComponent mPanel;
        private UITextComponent mVelocityText;
        private UITextComponent mAltitudeText;
        private UIButtonComponent mUnloadCargoButtonComponent;
        private UIButtonComponent mLoadCargoButtonComponent;
        private TransformComponent mPanelTransform;

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

        private bool mLegsDeployed = false;

        private Vector2 mPreviousPos;
        private bool mIsDragging;
        private float mTotalTime;

        private CargoState mCargoState = CargoState.None;

        void OnCreate()
        {
            mStarship = this;
            mRS1 = FindChildEntityByName(this.Name, "RaptorSea1");
            mRS2 = FindChildEntityByName(this.Name, "RaptorSea2");
            mRS3 = FindChildEntityByName(this.Name, "RaptorSea3");

            mStarshipInfoPanel = FindChildEntityByName(mStarship.GetComponent<TagComponent>().Tag, "InfoPopup");
            mPanel = mStarshipInfoPanel.GetComponent<UIPanelComponent>();
            mVelocityEntity = mStarshipInfoPanel.FindDecententByName(mStarshipInfoPanel.ID, "VelocityText");
            mAltitudeEntity = mStarshipInfoPanel.FindDecententByName(mStarshipInfoPanel.ID, "AltitudeText");
            mUnloadCargoButton = mStarshipInfoPanel.FindDecententByName(mStarshipInfoPanel.ID, "UnloadCargoButton");
            mLoadCargoButton = mStarshipInfoPanel.FindDecententByName(mStarshipInfoPanel.ID, "LoadCargoButton");
            mPanelTransform = mStarshipInfoPanel.GetComponent<TransformComponent>();
            mVelocityText = mVelocityEntity.GetComponent<UITextComponent>();
            mAltitudeText = mAltitudeEntity.GetComponent<UITextComponent>();
            mUnloadCargoButtonComponent = mUnloadCargoButton.GetComponent<UIButtonComponent>();
            mLoadCargoButtonComponent = mLoadCargoButton.GetComponent<UIButtonComponent>();
            mTotalTime = 0.0f;

            mRigidBody = this.GetComponent<RigidBodyComponent>();
            mShipTransform = this.GetComponent<TransformComponent>();
            mBoxCollider = this.GetComponent<BoxColliderComponent>();
            mMesh = this.GetComponent<MeshComponent>();

            mRaptorSea1TC = mRS1.GetComponent<TransformComponent>();
            mRaptorSea2TC = mRS2.GetComponent<TransformComponent>();
            mRaptorSea3TC = mRS3.GetComponent<TransformComponent>();

            mRS1Particles = mRS1.GetComponent<ParticlesComponent>();
            mRS2Particles = mRS2.GetComponent<ParticlesComponent>();
            mRS3Particles = mRS3.GetComponent<ParticlesComponent>();

            mCargoState = CargoState.None;

            mFullLifeTime = mRS1Particles.MaxLifeTime;
            mFullSpawnDelay = mRS1Particles.SpawnDelay;
            mFullStartIntensity = mRS1Particles.StartIntensity;
        }

        void OnEvent()
        {
            if (Input.IsMouseButtonPressed(MouseCode.ButtonLeft))
                mPanel.Visible = true;
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
                case LandingState.Landed:
                    break;
            }

            // Apply thrust from all active engines every frame
            ApplyThrust(ts);
            UpdateEngineRamp();

            // Info Panel
            if (mPanel.Visible)
            {
                Vector2 mousePos = Input.GetMousePosition();

                if (Input.IsMouseButtonPressed(MouseCode.ButtonLeft))
                {

                    if (!mIsDragging)
                    {
                        Vector3 panelTranslation = mPanelTransform.Translation;
                        Vector2 viewportSize = Scene.RenderTargetSize();

                        panelTranslation.X += (viewportSize.X * 0.5f);
                        panelTranslation.Y += (viewportSize.Y * 0.5f);

                        Vector3 panelScale = mPanelTransform.Scale;
                        float borderSize = 20.0f;

                        // Check if the mouse is over the panel to start dragging
                        if ((mousePos.X >= panelTranslation.X && mousePos.X <= (panelTranslation.X + panelScale.X)) && (mousePos.Y <= (panelTranslation.Y + borderSize) && mousePos.Y >= panelTranslation.Y))
                        {
                            mIsDragging = true;
                            mPreviousPos = mousePos;
                        }
                    }
                    else
                    {
                        // Calculate the delta movement
                        Vector2 deltaMousePos = mousePos - mPreviousPos;

                        // Update the panel's translation
                        mPanelTransform.Translation += new Vector3(deltaMousePos.X, deltaMousePos.Y, 0.0f);

                        // Update the previous mouse position
                        mPreviousPos = mousePos;
                    }
                }
                else
                    mIsDragging = false;

                if (landingState == LandingState.Landed && mCargoState == CargoState.None)
                {
                    mUnloadCargoButtonComponent.Visible = true;
                    mLoadCargoButtonComponent.Visible = false;
                }
                else if (landingState == LandingState.Landed && mCargoState == CargoState.UnloadingCargo)
                {
                    mUnloadCargoButtonComponent.Visible = false;
                    mLoadCargoButtonComponent.Visible = true;
                }
                else if (landingState == LandingState.Landed && mCargoState == CargoState.LoadingCargo)
                {
                    mUnloadCargoButtonComponent.Visible = true;
                    mLoadCargoButtonComponent.Visible = false;
                }

                if (mTotalTime > 0.15f)
                {
                    float linearVelocity = Vector3.Length(mRigidBody.LinearVelocity);

                    mVelocityText.Text = $"Velocity: {linearVelocity:F1} m/s";
                    mAltitudeText.Text = $"Altitude: {altitude:F1} m";
                }

                if (mTotalTime > 0.2f)
                    mTotalTime -= 0.2f;

                mTotalTime += ts;
            }
        }

        public void SetCargoState(CargoState state)
        {
            mCargoState = state;
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
            Vector3 velocity = mRigidBody.LinearVelocity;
            Vector3 radialUp = GetRadialUp();

            // Decompose velocity
            float radialComponent = velocity.X * radialUp.X + velocity.Y * radialUp.Y + velocity.Z * radialUp.Z;
            Vector3 lateralVelocity = new Vector3(velocity.X - radialUp.X * radialComponent, velocity.Y - radialUp.Y * radialComponent, velocity.Z - radialUp.Z * radialComponent);
            float lateralSpeed = (float)Math.Sqrt(lateralVelocity.X * lateralVelocity.X + lateralVelocity.Y * lateralVelocity.Y + lateralVelocity.Z * lateralVelocity.Z);

            // --- Single gimbal command ---
            float gimbalCommand = ArrestKp * pitch + ArrestKd * angVel;
            Vector3 lateralDir = new Vector3(flipAxis.Y * radialUp.Z - flipAxis.Z * radialUp.Y, flipAxis.Z * radialUp.X - flipAxis.X * radialUp.Z, flipAxis.X * radialUp.Y - flipAxis.Y * radialUp.X);
            float lateralInPlane = velocity.X * lateralDir.X + velocity.Y * lateralDir.Y + velocity.Z * lateralDir.Z;
            gimbalCommand += 0.4f * lateralInPlane;
            gimbalCommand = Clamp(gimbalCommand, -MaxArrestGimbal * 0.5f, MaxArrestGimbal * 0.5f);
            SetGimbalAngle(gimbalCommand); // single call

            // --- Direct lateral cancellation, scales with altitude ---
            if (lateralSpeed > 0.05f)
            {
                float altitudeFactor = 1.0f - Clamp(altitude / 100.0f, 0.0f, 1.0f);
                float cancelStrength = Lerp(0.3f, 1.0f, altitudeFactor); // full cancel near ground
                mRigidBody.LinearVelocity = new Vector3(mRigidBody.LinearVelocity.X - lateralVelocity.X * cancelStrength, mRigidBody.LinearVelocity.Y - lateralVelocity.Y * cancelStrength, mRigidBody.LinearVelocity.Z - lateralVelocity.Z * cancelStrength);
            }

            // --- Throttle ---
            float totalThrust = MaxThrustNewtons * ActiveEngineCount;
            if (altitude > 0.5f && descentSpeed > TargetTouchdownSpeed)
            {
                float desiredDecel = (descentSpeed * descentSpeed - TargetTouchdownSpeed * TargetTouchdownSpeed) / (2.0f * altitude);
                float requiredAccel = desiredDecel + gravity;
                float throttle = (requiredAccel * mRigidBody.Mass) / totalThrust;
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
                float hoverThrottle = gravity * mRigidBody.Mass / totalThrust;
                SetThrottle(hoverThrottle * 0.95f);
            }

            // --- Leg deployment ---
            if (!mLegsDeployed && altitude < LegDeployAltitude)
            {
                mMesh.PlayAnimation("LegUnfold");
                mBoxCollider.Size = new Vector3(8.0f, 25.954125f, 5.3f);
                mBoxCollider.Offset = new Vector3(0.0f, -0.70825f, 0.0f);
                mLegsDeployed = true;
                Toast.Console.LogInfo($"[Landing] Legs deploying at altitude={altitude:F0}m");
            }

            // --- Hard lateral cap — last thing before touchdown check ---
            if (altitude < 50.0f)
            {
                Vector3 vel = mRigidBody.LinearVelocity;
                Vector3 ru = GetRadialUp();
                float radial = vel.X * ru.X + vel.Y * ru.Y + vel.Z * ru.Z;
                Vector3 lateral = new Vector3(vel.X - ru.X * radial, vel.Y - ru.Y * radial, vel.Z - ru.Z * radial);
                float lateralMag = (float)Math.Sqrt(lateral.X * lateral.X + lateral.Y * lateral.Y + lateral.Z * lateral.Z);
                float t = 1.0f - Clamp(altitude / 50.0f, 0.0f, 1.0f);
                float maxLateral = Lerp(3.0f, 0.1f, t);
                if (lateralMag > maxLateral)
                {
                    float scale = maxLateral / lateralMag;
                    mRigidBody.LinearVelocity = new Vector3(ru.X * radial + lateral.X * scale, ru.Y * radial + lateral.Y * scale, ru.Z * radial + lateral.Z * scale);
                }
            }

            // --- Touchdown ---
            if (altitude < 0.5f && descentSpeed < TargetTouchdownSpeed + 1.0f)
            {
                Toast.Console.LogInfo($"[Landing] touchdown at speed={descentSpeed:F2}m/s pitch={pitch:F1}°");
                Toast.Console.LogInfo($"[Landing] linear velocity={mRigidBody.LinearVelocity:F2}m/s at touchdown");
                SetEngineActive(false);
                SetThrottle(0f);
                SetGimbalAngle(0f);
                mRigidBody.AngularDamping = 2.0f;
                TransitionTo(LandingState.Landed);
            }
        }

        private void TransitionTo(LandingState newState)
        {
            Toast.Console.LogInfo($"[Landing] Transitioning from {landingState} to {newState}");

            if (newState == LandingState.Landed)
                mLegsDeployed = false;

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
            if (active && !enginesActive)
                mIgnitionTime = GetTime();

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

        private void UpdateEngineRamp()
        {
            if (!enginesActive || mIgnitionTime < 0.0f)
                return;

            float t = Clamp((GetTime() - mIgnitionTime) / IgnitionRampTime, 0.0f, 1.0f);

            float lifeTime = Lerp(IgnitionLifeTimeStart, mFullLifeTime, t);
            float intensity = Lerp(IgnitionIntensityStart, mFullStartIntensity, t);

            // Density: fewer particles early. Guard against ever reaching 0
            float spawnDelay = Lerp(mFullSpawnDelay * IgnitionSpawnDelayMult, mFullSpawnDelay, t);
            spawnDelay = Math.Max(spawnDelay, 0.00001f);

            ApplyPlumeSettings(lifeTime, spawnDelay, intensity);
        }

        private void ApplyPlumeSettings(float lifeTime, float spawnDelay, float intensity)
        {
            mRS1Particles.MaxLifeTime = lifeTime;
            mRS2Particles.MaxLifeTime = lifeTime;
            mRS3Particles.MaxLifeTime = lifeTime;

            mRS1Particles.SpawnDelay = spawnDelay;
            mRS2Particles.SpawnDelay = spawnDelay;
            mRS3Particles.SpawnDelay = spawnDelay;

            mRS1Particles.StartIntensity = intensity;
            mRS2Particles.StartIntensity = intensity;
            mRS3Particles.StartIntensity = intensity;
        }
    }
}
using UnityEngine;
using UnityEngine.UI;
using System;
using System.Text;
using System.Net;
using System.Net.Sockets;
using System.Linq;

// Defines control types for the robot. Only autonomous modes remain.
public class ControlDevices
{
    public static string ChaseTarget = "chaseTarget";
    public static string FindAndChaseBall = "findAndChaseBall";
    public static string PushBallToGoal = "pushBallToGoal"; // AI to find ball, push to goal, with new behaviors
}

public class RobotController : Draggable
{
    private const float FULL_TORQUE = 3.6f;

    // Public variables for configuration in the Unity Inspector
    public string Control;
    public int Port = 0; // Port for external control (still exists but less relevant with autonomous AI)
    public float FlickerProbability = 0f;
    public float FlickerDuration = 0f;

    public WheelCollider[] rightWheels;
    public WheelCollider[] leftWheels;

    // AI specific variables
    public Transform DefaultTarget;
    public Transform GoalTransform; // This should point to the GameObject with the disappearance zone collider
    public float ChaseSpeed = 1.0f;
    public float TurnSpeed = 3.0f; // Default for proportional turning when generally aligned
    public float PureRotateAngleThreshold = 45f; // Angle in degrees for pure tank-like rotation
    public float ObstacleDetectionDistance = 5.0f;
    public LayerMask ObstacleLayer;
    public string BallTag = "Ball";
    public float PushDistanceBehindBall = 0.5f;
    public float AngleThresholdForForward = 15f; // Angle for applying full forward speed
    public float StopDistance = 0.5f;

    // Ball Interaction
    public float BallSlowDownFactor = 0.7f; // Speed multiplier when actively pushing the ball

    // Stuck Logic public variables
    public float StuckDetectionTime = 3.0f; // Time in seconds robot must be stationary to be considered stuck
    public float MovementThreshold = 0.05f; // Minimum movement required per FixedUpdate to not be stuck
    public float BackupDuration = 1.5f; // How long to back up when stuck
    public float BackupSpeed = 0.5f; // Speed multiplier when backing up
    public float BackupTurnSpeedMultiplier = 1.0f; // NEW: How fast to turn during backup

    // Private internal state variables
    private UdpClient _socket;
    private float _leftTorque = 0;
    private float _rightTorque = 0;
    private float _lastUpdate = 0; 
    private float _lastFlicker = 0;

    private Vector3 _currentPushTargetPosition;
    private bool _isTouchingBall = false; // Flag to know if robot is touching a ball
    private Transform _ballBeingPushed = null; // Reference to the ball currently being pushed

    // Stuck Logic private variables
    private Vector3 _lastFixedUpdatePosition; // Position at the last FixedUpdate
    private float _timeStuck; // Time robot has been stuck
    private bool _isStuck; // Is the robot currently detected as stuck?
    private bool _isBackingUp; // Is the robot currently in backup mode?
    private float _backupStartTime; // Time backup started

    private Collider _goalTriggerCollider; // Reference to the Goal's trigger collider (used for filtering balls)

    // Resets the motor torque to zero for all wheels and AI states.
    public void reset()
    {
        _leftTorque = 0;
        _rightTorque = 0;
        _isTouchingBall = false;
        _ballBeingPushed = null;

        // Reset stuck logic state
        _isStuck = false;
        _isBackingUp = false;
        _timeStuck = 0;
        _backupStartTime = 0;
    }

    // --- UDP Listening Methods (Unchanged) ---
    void ListenForUDP()
    {
        _socket.BeginReceive(new AsyncCallback(ReceiveData), new {});
    }
    float MapToRealWorldTorque(int commandTorque)
    {
        // the torque response in the real robot is not linear
        // TODO: make more accurate measurements and improve this
        // TODO: make the formula generic. This only (somewhat) works for the max torque value 3.6
        var torque = Utils.MapAndLimit(commandTorque, -100, 100, -FULL_TORQUE, FULL_TORQUE);
        var torqueSign = Math.Sign(commandTorque);
        var toreuqValue = Math.Abs(torque);
        toreuqValue = toreuqValue * 1.96f - 0.92f;
        toreuqValue = Utils.Limit((float)toreuqValue, 0, FULL_TORQUE);
        return torqueSign * toreuqValue;
    }
    void ParseCommand(string command)
    {
        var commandValues = command.Split(';');
        if (commandValues.Length > 1)
        {
            if (Int32.TryParse(commandValues[0], out var leftCommand) && Int32.TryParse(commandValues[1], out var rightCommand))
            {
                _leftTorque = MapToRealWorldTorque(leftCommand);
                _rightTorque = MapToRealWorldTorque(rightCommand);
            }
        }
    }
    void ReceiveData(IAsyncResult result)
    {
        try
        {
            var anyIP = new IPEndPoint(IPAddress.Any, 0);
            var data = _socket.EndReceive(result, ref anyIP);
            var command = Encoding.UTF8.GetString(data);
            Debug.Log($"RobotController received UDP message: {command}");
            ParseCommand(command);
            ListenForUDP();
        }
        catch (ObjectDisposedException)
        {
            // Socket was closed, no error needed.
        }
        catch (Exception e)
        {
            Debug.LogError($"RobotController UDP ReceiveData error: {e.Message}");
        }
    }

    // --- Removed Keyboard and Random Control Methods (no longer needed) ---

    // --- Core AI Logic for chasing a given target position ---
    void ApplyChaseLogic(Vector3 targetPosition, bool isCurrentlyPushingBall = false)
    {
        // If currently backing up, this method should not be called by FixedUpdate.
        // It's meant for forward movement and obstacle avoidance.
        // The backing up movement is handled directly in FixedUpdate for combined movement.
        if (_isBackingUp)
        {
            return;
        }

        Vector3 directionToTarget = (targetPosition - transform.position).normalized;
        float angleToTarget = Vector3.SignedAngle(transform.forward, directionToTarget, Vector3.up);
        float distanceToTarget = Vector3.Distance(transform.position, targetPosition);

        // Basic Obstacle Avoidance (Raycast forward)
        RaycastHit hit;
        Vector3 rayOrigin = transform.position + transform.forward * 0.5f;
        Debug.DrawRay(rayOrigin, transform.forward * ObstacleDetectionDistance, Color.red); // Robot's obstacle detection ray

        if (Physics.Raycast(rayOrigin, transform.forward, out hit, ObstacleDetectionDistance, ObstacleLayer))
        {
            Debug.Log($"Robot {gameObject.name}: Obstacle detected ({hit.collider.name}). Turning to avoid.");
            // When avoiding, always use pure rotation to turn quickly
            if (UnityEngine.Random.value > 0.5f) // Randomly choose left or right turn for avoidance
            {
                _leftTorque = FULL_TORQUE * TurnSpeed;
                _rightTorque = -FULL_TORQUE * TurnSpeed;
            }
            else
            {
                _leftTorque = -FULL_TORQUE * TurnSpeed;
                _rightTorque = FULL_TORQUE * TurnSpeed;
            }
            return; // Prioritize avoidance over chasing
        }

        float forwardTorque = 0;
        float turningTorque = 0;

        // --- Tank-like Turning Logic ---
        if (Mathf.Abs(angleToTarget) > PureRotateAngleThreshold)
        {
            // Pure rotation (tank-like turning)
            if (angleToTarget > 0) // Target is to the right
            {
                _leftTorque = FULL_TORQUE * TurnSpeed;
                _rightTorque = -FULL_TORQUE * TurnSpeed;
            }
            else // Target is to the left
            {
                _leftTorque = -FULL_TORQUE * TurnSpeed;
                _rightTorque = FULL_TORQUE * TurnSpeed;
            }
            forwardTorque = 0; // No forward movement during pure rotation
        }
        else // Within the pure rotation threshold, allow combined movement and proportional turning
        {
            // Calculate proportional turning torque
            float normalizedAngle = Mathf.Clamp(angleToTarget / 90f, -1f, 1f);
            turningTorque = normalizedAngle * FULL_TORQUE * TurnSpeed;

            // Calculate Forward/Backward Torque
            if (distanceToTarget > StopDistance || isCurrentlyPushingBall)
            {
                if (Mathf.Abs(angleToTarget) < AngleThresholdForForward)
                {
                    forwardTorque = FULL_TORQUE * ChaseSpeed;
                }
                else
                {
                    forwardTorque = FULL_TORQUE * ChaseSpeed * (1 - (Mathf.Abs(angleToTarget) / 180f));
                    forwardTorque = Mathf.Max(0, forwardTorque);
                }
            }
            
            // Apply combined torque
            _leftTorque = forwardTorque + turningTorque;
            _rightTorque = forwardTorque - turningTorque;
        }

        // Clamp total torque to FULL_TORQUE to prevent exceeding max capacity
        _leftTorque = Mathf.Clamp(_leftTorque, -FULL_TORQUE, FULL_TORQUE);
        _rightTorque = Mathf.Clamp(_rightTorque, -FULL_TORQUE, FULL_TORQUE);

        // Conditional Stop/Slow Down (only if NOT actively pushing a ball AND very close to the final target)
        if (!isCurrentlyPushingBall && distanceToTarget < StopDistance)
        {
            _leftTorque = 0;
            _rightTorque = 0;
            Debug.Log($"Robot {gameObject.name}: Close to final target ({targetPosition}). Stopping.");
        }
    }

    // --- AI Control Method: Chase Default Target (Unchanged) ---
    void ChaseDefaultTargetControl()
    {
        if (DefaultTarget == null)
        {
            Debug.LogWarning("RobotController: DefaultTarget is not assigned for ChaseTarget control.", this);
            _leftTorque = 0; _rightTorque = 0;
            return;
        }
        ApplyChaseLogic(DefaultTarget.position, false);
    }

    void FindAndChaseBallControl()
    {
        GameObject[] balls = GameObject.FindGameObjectsWithTag(BallTag);
        Transform closestBall = null;
        float minDistance = Mathf.Infinity;

        foreach (GameObject ball in balls)
        {
            float distance = Vector3.Distance(transform.position, ball.transform.position);
            if (distance < minDistance)
            {
                minDistance = distance;
                closestBall = ball.transform;
            }
        }

        if (closestBall != null)
        {
            ApplyChaseLogic(closestBall.position, false);
        }
        else // Fallback to DefaultTarget if no ball is found
        {
            Debug.LogWarning($"Robot {gameObject.name}: No objects with tag '{BallTag}' found. Chasing DefaultTarget instead.", this);
            if (DefaultTarget != null)
            {
                ApplyChaseLogic(DefaultTarget.position, false);
            } else {
                _leftTorque = 0; _rightTorque = 0;
            }
        }
    }

    // --- Modified AI Control Method: Push Ball to Goal ---
    void PushBallToGoalControl()
    {
        if (GoalTransform == null)
        {
            Debug.LogError($"Robot {gameObject.name}: GoalTransform is not assigned for PushBallToGoal control. Cannot push ball.", this);
            _leftTorque = 0; _rightTorque = 0;
            return;
        }
        
        // Ensure _goalTriggerCollider is correctly referenced for filtering balls in the goal area.
        // It should be the collider of the object that represents the 'disappearance zone' for the balls.
        // This is crucial for robots to ignore balls already scored/in the goal.
        if (_goalTriggerCollider == null) 
        {
             if (GoalTransform != null) {
                 _goalTriggerCollider = GoalTransform.GetComponent<Collider>();
                 if (_goalTriggerCollider == null) {
                    Debug.LogError($"Robot {gameObject.name}: GoalTransform ('{GoalTransform.name}') has no Collider component. Ball filtering will not work!", this);
                    // Fallback to not filtering if no collider found
                 } else if (!_goalTriggerCollider.isTrigger) {
                    Debug.LogWarning($"Robot {gameObject.name}: GoalTransform's collider ('{_goalTriggerCollider.name}') is not a trigger. Make sure 'Is Trigger' is checked in its inspector for goal filtering to work correctly.", this);
                 }
             }
        }

        GameObject[] activeBalls;
        if (_goalTriggerCollider != null && _goalTriggerCollider.isTrigger) // Only filter if GoalTransform has a valid trigger collider
        {
            // Find active balls, filtering out those inside the goal area
            activeBalls = GameObject.FindGameObjectsWithTag(BallTag)
                                                .Where(b => b.activeInHierarchy && !_goalTriggerCollider.bounds.Contains(b.transform.position)) // Filter balls in goal
                                                .ToArray();
        }
        else // If no valid goal collider for filtering, consider all active balls
        {
            activeBalls = GameObject.FindGameObjectsWithTag(BallTag)
                                                .Where(b => b.activeInHierarchy)
                                                .ToArray();
            if (GoalTransform != null) Debug.LogWarning($"Robot {gameObject.name}: Not filtering balls in goal because GoalTransform's collider is missing or not a trigger.");
        }


        HandleBallPushing(activeBalls);
    }

    // Helper method to abstract ball pushing logic
    private void HandleBallPushing(GameObject[] currentBalls)
    {
        Transform nearestBall = null; 
        float minDistance = Mathf.Infinity;

        // Find the absolute nearest ball from the filtered list
        foreach (GameObject ball in currentBalls)
        {
            float distance = Vector3.Distance(transform.position, ball.transform.position);
            if (distance < minDistance)
            {
                minDistance = distance;
                nearestBall = ball.transform;
            }
        }

        // Proceed with pushing the nearestBall (or moving to goal if no ball)
        if (nearestBall != null)
        {
            if (_isTouchingBall && _ballBeingPushed == nearestBall) // Robot is currently touching the nearest ball
            {
                Debug.Log($"Robot {gameObject.name}: Actively pushing {nearestBall.name} towards goal.");
                // When touching, directly push the ball towards the goal.
                // The target is the GoalTransform, and we apply slowdown in ApplyChaseLogic
                ApplyChaseLogic(GoalTransform.position, true); // true because it's actively pushing
            }
            else // Not touching the nearest ball, or touching a different ball
            {
                // Go to position behind the nearest ball to get ready to push.
                // This will maintain higher speed until it reaches the push position and makes contact.
                Vector3 directionFromBallToGoal = (GoalTransform.position - nearestBall.position).normalized;
                _currentPushTargetPosition = nearestBall.position - directionFromBallToGoal * PushDistanceBehindBall;

                Debug.DrawLine(nearestBall.position, _currentPushTargetPosition, Color.green, 0.1f); // Robot's target (green)
                Debug.DrawLine(nearestBall.position, GoalTransform.position, Color.yellow, 0.1f); // Ball to Goal direction (yellow)

                ApplyChaseLogic(_currentPushTargetPosition, false); // false because it's approaching, not actively pushing yet
            }
        }
        else // If no active balls found, robot moves towards the default target
        {
            Debug.Log($"Robot {gameObject.name}: No active balls. Moving towards DefaultTarget.");
            if (DefaultTarget != null)
            {
                 ApplyChaseLogic(DefaultTarget.position, false); // Not pushing a ball, so it can stop at DefaultTarget
            }
            else
            {
                _leftTorque = 0;
                _rightTorque = 0;
                Debug.LogWarning($"Robot {gameObject.name}: No DefaultTarget assigned and no balls. Stopping.");
            }
        }
    }


    // Handles visual flickering for the Aruco marker on the robot (Unchanged)
    void handleArucoFlicker() {
        if (FlickerProbability > 0 && UnityEngine.Random.Range(0f, 1f) < FlickerProbability) {
            _lastFlicker = Time.time;
        }
        if (_lastFlicker > 0) {
            Transform markerTransform = transform.Find("Marker");
            if (markerTransform != null) {
                Renderer markerRenderer = markerTransform.GetComponent<Renderer>();
                if (markerRenderer != null) {
                    markerRenderer.enabled = (Time.time - _lastFlicker) >= FlickerDuration;
                    if (!markerRenderer.enabled) {
                        // Marker is currently invisible due to flicker
                    } else {
                        _lastFlicker = 0; // Flicker duration ended, reset
                    }
                }
            }
        }
    }

    // Called once when the script instance is being loaded.
    public override void Start()
    {
        base.Start();

        if (Port > 0)
        {
            try {
                _socket = new UdpClient(Port);
                Debug.Log($"Robot {gameObject.name}: Listening for UDP packets on port: {Port}");
                ListenForUDP();
            } catch (SocketException e) {
                Debug.LogError($"Robot {gameObject.name}: Failed to start UDP listener on port {Port}: {e.Message}");
            }
        }
        IgnoreWheelCollisionsWithBalls();
        
        // Initialize for unsticking logic
        _lastFixedUpdatePosition = transform.position; 

        // Get the Goal's trigger collider for ball filtering. This must be the collider of the object
        // that defines the "disappearance zone" and also the area robots should ignore.
        if (GoalTransform != null)
        {
            _goalTriggerCollider = GoalTransform.GetComponent<Collider>();
            if (_goalTriggerCollider == null)
            {
                Debug.LogError($"Robot {gameObject.name}: GoalTransform ('{GoalTransform.name}') has no Collider component. Ball filtering will not work!", this);
            }
            else if (!_goalTriggerCollider.isTrigger)
            {
                Debug.LogWarning($"Robot {gameObject.name}: GoalTransform's collider ('{_goalTriggerCollider.name}') is not a trigger. Make sure 'Is Trigger' is checked in its inspector for goal filtering to work correctly.", this);
            }
        }
        else
        {
            Debug.LogError($"Robot {gameObject.name}: GoalTransform is not assigned in the Inspector. Goal filtering will not work!");
        }
    }

    // Configures physics to ignore collisions between the robot's wheels and "dynamic-object" tagged GameObjects (Unchanged)
    public void IgnoreWheelCollisionsWithBalls()
    {
        var wheelColliders = leftWheels.Concat(rightWheels).ToArray();
        foreach (var dynamicObject in GameObject.FindGameObjectsWithTag("dynamic-object")) {
            foreach (WheelCollider wheelCollider in wheelColliders)
            {
                Collider dynamicCollider = dynamicObject.GetComponent<Collider>();
                if (dynamicCollider != null && wheelCollider != null) {
                    Physics.IgnoreCollision(dynamicCollider, wheelCollider);
                }
            }
        }
    }

    // Called every frame.
    void Update()
    {
        // Debugging Aid: Draw the robot's forward direction (Blue line)
        Debug.DrawRay(transform.position, transform.forward * 2f, Color.blue);

        switch (Control)
        {
            case "chaseTarget":
                ChaseDefaultTargetControl();
                break;
            case "findAndChaseBall":
                FindAndChaseBallControl();
                break;
            case "pushBallToGoal":
                PushBallToGoalControl();
                break;
            default: // Default case for any unsupported control or initial state
                _leftTorque = 0;
                _rightTorque = 0;
                break;
        }

        handleArucoFlicker();

        foreach (WheelCollider wheelCollider in leftWheels)
        {
            wheelCollider.motorTorque = _leftTorque;
        }
        foreach (WheelCollider wheelCollider in rightWheels)
        {
            wheelCollider.motorTorque = _rightTorque;
        }
    }

    // FixedUpdate for physics-based unsticking and combined backup-turn
    void FixedUpdate()
    {
        // Only run unsticking logic if using autonomous AI control
        if (Control != ControlDevices.PushBallToGoal && Control != ControlDevices.ChaseTarget && Control != ControlDevices.FindAndChaseBall)
        {
            // Reset stuck state if not in an AI mode that can get stuck
            _isStuck = false;
            _isBackingUp = false;
            _timeStuck = 0;
            _lastFixedUpdatePosition = transform.position;
            return;
        }

        // If currently backing up, apply combined reverse and turning torque
        if (_isBackingUp)
        {
            // Check if backup duration is over
            if (Time.time - _backupStartTime >= BackupDuration)
            {
                _isBackingUp = false; // Stop backing up
                _isStuck = false; // Reset stuck state
                _timeStuck = 0; // Reset stuck timer
                
                // After backing up, force re-evaluation of target by clearing _ballBeingPushed
                _ballBeingPushed = null; 
                _isTouchingBall = false; // Ensure this is also reset

                Debug.Log($"Robot {gameObject.name}: Finished backing up. Resuming normal movement.");
            }
            else
            {
                // Find the nearest ball to turn towards while backing up
                Transform nearestBall = null;
                GameObject[] activeBalls = GameObject.FindGameObjectsWithTag(BallTag)
                                                    .Where(b => b.activeInHierarchy && (_goalTriggerCollider == null || !_goalTriggerCollider.bounds.Contains(b.transform.position)))
                                                    .ToArray();
                float minDistance = Mathf.Infinity;
                foreach (GameObject ball in activeBalls)
                {
                    float distance = Vector3.Distance(transform.position, ball.transform.position);
                    if (distance < minDistance)
                    {
                        minDistance = distance;
                        nearestBall = ball.transform;
                    }
                }

                float backupTurningTorque = 0;
                if (nearestBall != null)
                {
                    // Calculate angle to nearest ball (for turning while backing up)
                    // We want to turn the *front* of the robot towards the ball, even while reversing
                    Vector3 directionToBall = (nearestBall.position - transform.position).normalized;
                    float angleToBall = Vector3.SignedAngle(transform.forward, directionToBall, Vector3.up);
                    
                    float normalizedAngle = Mathf.Clamp(angleToBall / 90f, -1f, 1f);
                    backupTurningTorque = normalizedAngle * FULL_TORQUE * BackupTurnSpeedMultiplier;

                    Debug.DrawLine(transform.position, nearestBall.position, Color.cyan, 0.1f); // Debug line to ball while backing up
                }
                
                // Apply reverse torque and turning torque
                _leftTorque = (-FULL_TORQUE * BackupSpeed) + backupTurningTorque;
                _rightTorque = (-FULL_TORQUE * BackupSpeed) - backupTurningTorque;

                // Clamp total torque
                _leftTorque = Mathf.Clamp(_leftTorque, -FULL_TORQUE, FULL_TORQUE);
                _rightTorque = Mathf.Clamp(_rightTorque, -FULL_TORQUE, FULL_TORQUE);
            }
            return; // Still backing up, so exit FixedUpdate after applying torque
        }

        // Check for stuckness (only if not currently backing up)
        // Only check stuck if attempting to move (has non-zero torque).
        bool isAttemptingToMove = Mathf.Abs(_leftTorque) > 0.1f || Mathf.Abs(_rightTorque) > 0.1f;

        float movementSinceLastFixedUpdate = Vector3.Distance(transform.position, _lastFixedUpdatePosition);

        if (isAttemptingToMove && movementSinceLastFixedUpdate < MovementThreshold) // Only check stuck if trying to move
        {
            // If not moving enough, increment stuck timer
            _timeStuck += Time.fixedDeltaTime;
            if (!_isStuck && _timeStuck >= StuckDetectionTime) // StuckDetectionTime is 3.0f
            {
                _isStuck = true; // Mark as stuck
                _isBackingUp = true; // Start backing up
                _backupStartTime = Time.time; // Record backup start time
                Debug.LogWarning($"Robot {gameObject.name}: Detected as stuck! Initiating backup for {BackupDuration} seconds.");
            }
        }
        else
        {
            // If moving sufficiently or not attempting to move, reset stuck state
            _timeStuck = 0;
            _isStuck = false;
        }

        _lastFixedUpdatePosition = transform.position; // Update last position for next check
    }


    // Collision detection to know when touching a ball
    void OnCollisionEnter(Collision collision)
    {
        if (collision.gameObject.CompareTag(BallTag))
        {
            _isTouchingBall = true;
            _ballBeingPushed = collision.gameObject.transform;
            Debug.Log($"Robot {gameObject.name}: Touched ball: {collision.gameObject.name}");
        }
    }

    // Collision detection for continuous contact
    void OnCollisionStay(Collision collision)
    {
        if (collision.gameObject.CompareTag(BallTag))
        {
            _isTouchingBall = true; // Confirm still touching
            _ballBeingPushed = collision.gameObject.transform; // Ensure we have the reference
        }
    }

    // Collision detection for losing contact
    void OnCollisionExit(Collision collision)
    {
        if (collision.gameObject.CompareTag(BallTag))
        {
            _isTouchingBall = false;
            if (_ballBeingPushed == collision.gameObject.transform) // Only clear if it's the ball we were pushing
            {
                _ballBeingPushed = null;
            }
            Debug.Log($"Robot {gameObject.name}: Lost contact with ball: {collision.gameObject.name}");
        }
    }


    // Stops UDP listening and closes the socket (Unchanged)
    public void Stop()
    {
        _socket?.Close();
        if (_socket != null)
        {
            Debug.Log($"Robot {gameObject.name}: UDP Socket closed.");
        }
    }

    // Called when the script component is destroyed (Unchanged)
    void OnDestroy()
    {
        Stop();
    }
}


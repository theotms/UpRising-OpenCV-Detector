using UnityEngine;

[CreateAssetMenu(fileName = "NewAIProfile", menuName = "AI/AI Profile")]
public class AIProfile : ScriptableObject
{
    [Header("Movement & Turning")]
    public float ChaseSpeed = 1.0f;
    public float TurnSpeed = 3.0f;
    public float PureRotateAngleThreshold = 45f;
    public float PushDistanceBehindBall = 0.5f;
    public float AngleThresholdForForward = 15f;
    public float StopDistance = 0.5f;
    public float BallSlowDownFactor = 0.7f;

    [Header("Obstacle Avoidance")]
    public float ObstacleDetectionDistance = 5.0f;
    public LayerMask ObstacleLayer;

    [Header("Stuck Logic")]
    public float StuckDetectionTime = 3.0f;
    public float MovementThreshold = 0.05f;
    public float BackupDuration = 1.5f;
    public float BackupSpeed = 0.5f;
    public float BackupTurnSpeedMultiplier = 1.0f;
}


using UnityEngine;
using System;
using System.IO;
using System.Threading;
using System.Text;
using System.Net;
using System.Net.Sockets;
using System.Linq;

public class MainController : MonoBehaviour
{
    // Public GameObject references for your prefabs.
    // These are kept here in case you have other logic that still needs access to them,
    // but they are no longer used by this script for auto-spawning.
    public GameObject BallPrefab;
    public GameObject RobotPrefab;
    public GameObject TrafficConePrefab;
    public GameObject CubePrefab;
    public GameObject StreamCamera; // Reference to the camera GameObject for streaming

    GameObject _selectedObject;
    GameObject _highlightedObject;
    UdpClient _socket;
    string _command;
    string _configurationFilePath = "configuration.json";

    // These arrays will remain empty by default unless populated by a *different* mechanism (e.g., finding existing objects)
    // They were previously used to keep track of spawned objects. Now, for pre-placed objects, you'll manage them directly in the scene.
    GameObject[] _dynamicObjects = {};
    GameObject[] _robots = {};

    Configuration _configuration; // Holds settings loaded from configuration.json
    float _startedAt; // Timestamp for simulation duration

    // Initializes UDP listener for commands
    void ListenForUDP()
    {
        _socket.BeginReceive(new AsyncCallback(ReceiveData), new {});
    }

    // Callback for received UDP data
    void ReceiveData(IAsyncResult result)
    {
        var anyIP = new IPEndPoint(IPAddress.Any, 0);
        var data = _socket.EndReceive(result, ref anyIP);
        _command = Encoding.UTF8.GetString(data);
        Debug.Log($"Main controller received UDP message: {_command}");
        ListenForUDP(); // Continue listening
    }

    // Resets the simulation state. For pre-placed objects, this means resetting scores
    // and potentially the objects themselves if they have their own reset methods.
    void ResetSimulation()
    {
        // Reset scores on any GoalController in the scene
        foreach (var goal in GameObject.FindGameObjectsWithTag("goal"))
        {
            var goalController = goal.GetComponent<GoalController>();
            if (goalController != null)
            {
                goalController.SetScore(0);
            }
        }

        // We assume objects are already in the scene.
        // Instead of re-spawning, we will call a "reset" method on their controllers.
        // This clears any temporary spawned objects from previous runs/resets, but NOT your manually placed ones.
        SpawnConfigurationObjects(); // This method is now modified to *not* spawn.

        // Reset positions/states of manually placed robots and dynamic objects
        foreach (var robotController in FindObjectsOfType<RobotController>())
        {
            robotController.reset(); // Resets their motor torque
            // If you want to reset their *position* on reset, you'd add that logic here
            // (e.g., store initial positions or define specific reset points).
            // Example: robotController.transform.position = initialRobotPosition;
            // robotController.transform.rotation = initialRobotRotation;
        }
        foreach (var dynamicObjectController in FindObjectsOfType<DynamicObjectController>())
        {
            // If DynamicObjectController has a reset method, call it here.
            // dynamicObjectController.reset();
        }

        _startedAt = Time.time; // Reset simulation timer
    }

    // --- Spawn-related methods (kept for reference, but their calls are removed for auto-spawning) ---
    // These methods would only be used if you decide to implement custom in-game spawning later.
    GameObject SpawnPrefab(GameObject prefab, string hexColor)
    {
        var newObject = Instantiate(prefab);
        var color = new Color(1, 1, 1);
        ColorUtility.TryParseHtmlString(hexColor, out color);
        var renderer = newObject.GetComponent<MeshRenderer>();
        if (renderer)
        {
            renderer.material.color = color;
        }
        return newObject;
    }

    GameObject GetPrefab(string type)
    {
        switch (type)
        {
            case "traffic-cone":
                return TrafficConePrefab;
            case "cube":
                return CubePrefab;
            default:
                return BallPrefab;
        }
    }

    // These methods will NOT be actively used for auto-spawning in this modified MainController.
    // They remain here in case they are called by other parts of your project or if you re-introduce spawning.
    void SpawnDynamicObjects(DynamicObject[] dynamicObjects)
    {
        Debug.Log("MainController: SpawnDynamicObjects called, but objects are assumed to be pre-placed. No new objects instantiated.");
        // If you were to enable spawning, you'd iterate and call SpawnDynamicObject here.
    }

    GameObject SpawnDynamicObject(DynamicObject dynamicObject)
    {
        Debug.Log($"MainController: SpawnDynamicObject called for type {dynamicObject.type}, but no new object instantiated.");
        // This method would typically instantiate and configure a dynamic object.
        // For pre-placed objects, you would typically find existing ones and apply settings.
        return null; // Return null as no object is instantiated
    }

    string getAbsolutePath(string filePath, string subFolder = "")
    {
        return $"{Application.dataPath}/{subFolder}{filePath}".Replace('/', Path.DirectorySeparatorChar);
    }

    Texture2D LoadTexture(string filePath)
    {
        var texture = new Texture2D(256, 256);
        var absolutePath = getAbsolutePath(filePath);
        // On different systems the image folder might be on a parent folder
        if (!File.Exists(absolutePath))
        {
            absolutePath = getAbsolutePath(filePath, "../");
        }

        if (File.Exists(absolutePath))
        {
            var image = File.ReadAllBytes(absolutePath);
            texture.LoadImage(image);
        }
        return texture;
    }

    // These methods will NOT be actively used for auto-spawning in this modified MainController.
    void SpawnRobots(Robot[] robots)
    {
        Debug.Log("MainController: SpawnRobots called, but robots are assumed to be pre-placed. No new robots instantiated.");
        // If you were to enable spawning, you'd iterate and call SpawnRobot here.
    }

    GameObject SpawnRobot(Robot robot)
    {
        Debug.Log($"MainController: SpawnRobot called for marker {robot.marker}, but no new robot instantiated.");
        // This method would typically instantiate and configure a robot.
        // For pre-placed objects, you would typically find existing ones and apply settings.
        return null; // Return null as no robot is instantiated
    }

    // --- Utility methods for position and camera ---
    float[] GetPosition(GameObject gameObject)
    {
        var position = gameObject.transform.position;
        return new [] { position.x, position.y, position.z, gameObject.transform.eulerAngles.y };
    }

    void SetPosition(GameObject gameObject, float[] position)
    {
        var rigidbody = gameObject.GetComponent<Rigidbody>();
        if (rigidbody != null) // Ensure Rigidbody exists before trying to access it
        {
            rigidbody.velocity = new Vector3(0,0,0);
            rigidbody.angularVelocity = new Vector3(0,0,0);
        }
        gameObject.transform.position = new Vector3(position[0], position[1], position[2]);
        if (position.Length == 4)
        {
            gameObject.transform.rotation = Quaternion.Euler(0, position[3], 0);
        }
    }

    void SetCameraOptions(Configuration configuration)
    {
        if (StreamCamera == null)
        {
            Debug.LogWarning("StreamCamera GameObject is not assigned in MainController. Cannot set camera options.");
            return;
        }
        StreamCameraController cameraController = StreamCamera.GetComponent<StreamCameraController>();
        if (cameraController != null)
        {
            cameraController.SetCameraOffset(configuration.cameraOffset);
            cameraController.FrameInterval = 1f / configuration.streamFPS;
            cameraController.Width = configuration.streamWidth;
            cameraController.Height = configuration.streamHeight;
            cameraController.StartVideoServer(configuration.streamPort);
        } else {
            Debug.LogWarning("StreamCameraController component not found on StreamCamera GameObject.");
        }
    }

    // Starts UDP server for remote control commands
    void StartUDPServer(int port)
    {
        if (port > 0)
        {
            try {
                _socket = new UdpClient(port);
                Debug.Log($"Listening for UDP packets on port: {port}");
                ListenForUDP();
            } catch (SocketException e) {
                Debug.LogError($"Failed to start UDP listener on port {port}: {e.Message}");
            }
        }
    }

    // Saves current positions of objects (if they are tracked by _dynamicObjects / _robots arrays).
    // For manually placed objects, you will mostly rely on Unity's scene saving.
    void SaveConfiguration()
    {
        // IMPORTANT: This 'SaveConfiguration' will only save the positions of objects
        // that are currently referenced in the _dynamicObjects and _robots arrays.
        // Since we're no longer auto-populating these arrays with spawned objects,
        // this method will not automatically save the positions of your *manually placed* objects.
        // To save manually placed objects, you would need to:
        // 1. Find all robots (e.g., using GameObject.FindGameObjectsWithTag("Robot"))
        // 2. Create `Robot` and `DynamicObject` instances from them.
        // 3. Populate a new `Configuration` object.
        // For development, rely on Unity's File -> Save Scene.
        Debug.Log("MainController: Saving configuration. Note that this only saves properties of objects " +
                  "currently tracked in _dynamicObjects and _robots arrays, which might be empty if objects are pre-placed.");
        if (_configuration != null)
        {
            // This part assumes _configuration.dynamicObjects and _robots were correctly initialized
            // with enough capacity or are being re-created. Without auto-spawning populating these
            // arrays, this part might not work as expected for manually placed objects.
            for (int index = 0; index < _dynamicObjects.Length; index++)
            {
                if (_dynamicObjects[index] != null)
                {
                    _configuration.dynamicObjects[index].position = GetPosition(_dynamicObjects[index]);
                }
            }
            for (int index = 0; index < _robots.Length; index++)
            {
                if (_robots[index] != null)
                {
                    _configuration.robots[index].position = GetPosition(_robots[index]);
                }
            }
            File.WriteAllText(_configurationFilePath, JsonUtility.ToJson(_configuration));
            Debug.Log($"Configuration saved to: {_configurationFilePath}");
        } else {
            Debug.LogWarning("Cannot save configuration: _configuration object is null.");
        }
    }

    // Opens configuration from file. Does NOT re-spawn objects.
    void OpenConfiguration()
    {
        LoadConfiguration(_configurationFilePath);
        // This will now clear any temporary spawned objects from previous runs, but
        // it will *not* respawn anything. If you want to apply settings from the loaded config
        // to pre-placed objects, you'd need additional logic here to find and configure them.
        SpawnConfigurationObjects(); // This method is modified to *not* spawn.
        Debug.Log("MainController: Configuration opened. No objects were re-spawned.");
    }

    // Loads configuration data from a JSON file.
    void LoadConfiguration(string filePath)
    {
        try {
            var jsonString = File.ReadAllText(filePath);
            _configuration = JsonUtility.FromJson<Configuration>(jsonString);
            // Ensure width/height are set, defaulting to resolution if not explicitly defined
            _configuration.streamWidth = _configuration.streamWidth > 0 ? _configuration.streamWidth : _configuration.streamResolution;
            _configuration.streamHeight = _configuration.streamHeight > 0 ? _configuration.streamHeight : _configuration.streamResolution;
            Debug.Log($"Configuration loaded from: {filePath}");
        } catch (FileNotFoundException) {
            Debug.LogError($"Configuration file not found at: {filePath}. Using default/empty configuration.");
            _configuration = new Configuration(); // Create a default empty config to avoid null reference errors
        } catch (Exception e) {
            Debug.LogError($"Error loading configuration from {filePath}: {e.Message}");
            _configuration = new Configuration(); // Fallback to empty config on other errors
        }
    }

    // Clears previously spawned objects. Does NOT re-spawn for pre-placed scene setup.
    void SpawnConfigurationObjects()
    {
        _highlightedObject = null;
        _selectedObject = null;

        // Destroy any objects that might have been created by previous runs/resets
        // and stored in these internal arrays. This will *not* affect your manually placed
        // GameObjects in the scene hierarchy unless they somehow got added to these arrays.
        foreach (var item in _dynamicObjects.Concat(_robots).ToArray())
        {
            if (item != null)
            {
                // Safely stop RobotController before destroying its GameObject
                item.GetComponent<RobotController>()?.Stop();
                Destroy(item);
            }
        }
        _dynamicObjects = new GameObject[]{}; // Clear internal tracking arrays
        _robots = new GameObject[]{};

        // *** THESE LINES ARE COMMENTED OUT TO PREVENT AUTO-SPAWNING ***
        // SpawnDynamicObjects(_configuration.dynamicObjects);
        // SpawnRobots(_configuration.robots);

        Debug.Log("MainController: SpawnConfigurationObjects completed. Auto-spawning is disabled. " +
                  "Ensure your robots and dynamic objects are pre-placed in the scene.");
    }

    // Called once when the script instance is being loaded.
    void Start()
    {
        // Determine the correct path for the configuration file
        var configDefaultLocation = $"{Environment.CurrentDirectory}{Path.DirectorySeparatorChar}{_configurationFilePath}";
        var configSecondaryLocation = getAbsolutePath(_configurationFilePath, "../");
        Debug.Log($"Application.dataPath: {Application.dataPath}");
        Debug.Log($"Config default location: {configDefaultLocation}");
        Debug.Log($"Config secondary location: {configSecondaryLocation}");

        // Load configuration from file
        LoadConfiguration(File.Exists(configDefaultLocation) ? configDefaultLocation : configSecondaryLocation);

        // *** IMPORTANT: The line below is commented out to prevent auto-spawning at start. ***
        // ResetSimulation(); // This would call SpawnConfigurationObjects, triggering the spawning chain.

        // Apply global simulation settings from the loaded configuration
        Time.timeScale = _configuration.timeScale;
        QualitySettings.SetQualityLevel(_configuration.quality, true);
        // Set resolution. Consider if you want this to override editor window size in play mode.
        Screen.SetResolution(_configuration.streamWidth, _configuration.streamHeight, false);
        SetCameraOptions(_configuration); // Set up camera for streaming
        StartUDPServer(_configuration.controlPort); // Start UDP command server

        // Initialize/reset states of pre-placed objects in the scene.
        // This is crucial for your "always there" workflow.
        foreach (var robotController in FindObjectsOfType<RobotController>())
        {
            robotController.reset(); // Call the reset method on each RobotController
        }
        foreach (var dynamicObjectController in FindObjectsOfType<DynamicObjectController>())
        {
            // If DynamicObjectController has a public reset method, call it here.
            // dynamicObjectController.reset();
        }

        _startedAt = Time.time; // Initialize simulation timer
        Debug.Log("MainController started. Objects assumed to be pre-placed in scene.");
    }

    // Called every frame
    void Update ()
    {
        // Handle application exit via Escape key
        if (Input.GetKey("escape"))
        {
            Application.Quit();
        }
        // Handle "reset" command from UDP or 'q' key
        // This will trigger ResetSimulation, which now focuses on resetting states, not re-spawning.
        if (_command == "reset" || Input.GetKey("q"))
        {
            ResetSimulation();
            _command = null; // Clear the command after processing
        }

        // --- Mouse Interaction Logic (Dragging/Highlighting objects) ---
        var ray = Camera.main.ScreenPointToRay(Input.mousePosition);
        var mouseDown = Input.GetMouseButtonDown(0) || Input.GetMouseButtonDown(1);
        var mouseUp = Input.GetMouseButtonUp(0) || Input.GetMouseButtonUp(1);
        RaycastHit hit;

        if (Physics.Raycast(ray, out hit))
        {
            var target = hit.collider.gameObject;
            // Select object on mouse down
            if (_selectedObject == null && mouseDown)
            {
                _selectedObject = target;
            }
            // Deselect object on mouse up
            if (mouseUp)
            {
                _selectedObject = null;
            }

            // Reset highlight of previously highlighted object
            if (_highlightedObject != null && _highlightedObject.GetComponent<Draggable>() != null)
            {
                _highlightedObject.GetComponent<Draggable>().ResetHighlight();
            }
            
            // Set current highlighted object (either selected or new target)
            _highlightedObject = _selectedObject != null ? _selectedObject : target;
            var controller = _highlightedObject?.GetComponent<Draggable>();

            if (_highlightedObject != null && controller != null)
            {
                controller.Highlight();
                if (Input.GetMouseButton(0)) // Left mouse button for dragging
                {
                    controller.Drag(hit.point);
                }
                if (Input.GetMouseButton(1)) // Right mouse button for pointing
                {
                    controller.PointAt(hit.point);
                }
            }
        }
        // --- End Mouse Interaction Logic ---

        // Handle saving/opening configuration files via Ctrl+S / Ctrl+O
        var isControlPressed = Input.GetKey(KeyCode.LeftControl) || Input.GetKey(KeyCode.RightControl) ||
            Input.GetKey(KeyCode.LeftApple) || Input.GetKey(KeyCode.RightApple); // Added LeftApple/RightApple for Mac Cmd key
        if (isControlPressed && Input.GetKeyDown(KeyCode.S))
        {
            SaveConfiguration();
        }
        if (isControlPressed && Input.GetKeyDown(KeyCode.O))
        {
            OpenConfiguration();
        }

        // Handle delayed activation for dynamic objects (if any are manually configured this way)
        var elapsed = Time.time - _startedAt;
        // This loop iterates over _dynamicObjects which will be empty if not populated by spawning.
        // If you want to apply delay to manually placed balls, you'd need to find them (e.g., using FindGameObjectsWithTag)
        // and then get their DynamicObjectController.
        foreach (var dynamicObject in _dynamicObjects) // This array will remain empty.
        {
            if (dynamicObject == null)
            {
                continue;
            }
            var controller = dynamicObject.GetComponent<DynamicObjectController>();
            if (controller != null && controller.delay > 0)
            {
                dynamicObject.SetActive(elapsed > controller.delay);
            }
        }
    }

    // Called when the application quits, ensures UDP socket is closed.
    void OnApplicationQuit()
    {
        _socket?.Close();
        Debug.Log("MainController: UDP Socket closed on application quit.");
    }
}

// --- Serializable classes for Configuration, DynamicObject, and Robot ---
// These classes define the structure of your configuration.json file.
// They are NOT directly involved in spawning objects in this modified setup,
// but they define the data that can be loaded/saved.
[System.Serializable]
public class DynamicObject
{
    public string type;
    public string color;
    public float[] position;
    public float mass;
    public float size;
    public int value;
    public int delay;
}
[System.Serializable]
public class Robot
{
    public string marker;
    public float flickerProbability;
    public float flickerDuration;
    public string color;
    public string control;
    public float[] position;
}
[System.Serializable]
public class Configuration
{
    public int quality;
    public float timeScale;
    public int controlPort;
    public int streamFPS;
    public int streamResolution;
    public int streamWidth;
    public int streamHeight;
    public int streamPort;
    public float[] cameraOffset;
    public Robot[] robots; // Defines structure for robots in config file
    public DynamicObject[] dynamicObjects; // Defines structure for dynamic objects in config file
}


#pragma once

#include "CoreMinimal.h"
#include "Json.h"

class FUECLIToolRegistry;

/**
 * Handler class for material-related UECLI commands.
 * Handles material creation, material instances, and material parameter manipulation.
 */
class UECLI_API FUECLIMaterialCommands
{
public:
    FUECLIMaterialCommands();
    static void RegisterTools(FUECLIToolRegistry& Registry);

    // Handle material commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * Create a new material
     * @param Params - Must include:
     *                "name" - Material name
     *                "path" - Optional, destination folder (default: "/Game/Materials")
     * @return JSON response with created material details
     */
    TSharedPtr<FJsonObject> HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);

    /**
     * Create a new material function asset
     * @param Params - Must include:
     *                "name" - Function name
     *                "path" - Optional, destination folder (default: "/Game/MaterialFunctions")
     * @return JSON response with created function details
     */
    TSharedPtr<FJsonObject> HandleCreateMaterialFunction(const TSharedPtr<FJsonObject>& Params);

    /**
     * Create a material instance from a parent material
     * @param Params - Must include:
     *                "name" - Instance name
     *                "parent_material" - Path to parent material
     *                "path" - Optional, destination folder
     * @return JSON response with created instance details
     */
    TSharedPtr<FJsonObject> HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params);

    /**
     * Get material parameters
     * @param Params - Must include:
     *                "material_path" - Path to material or material instance
     * @return JSON object with all material parameters
     */
    TSharedPtr<FJsonObject> HandleGetMaterialParameters(const TSharedPtr<FJsonObject>& Params);

    /**
     * Set a material instance parameter
     * @param Params - Must include:
     *                "instance_path" - Path to material instance
     *                "parameter_name" - Name of the parameter to set
     *                "parameter_type" - Type: "Scalar", "Vector", "Texture"
     *                "value" - Value to set
     * @return JSON response with result
     */
    TSharedPtr<FJsonObject> HandleSetMaterialParameter(const TSharedPtr<FJsonObject>& Params);

    /**
     * List all materials in a folder
     * @param Params - Optional:
     *                "path" - Folder path (default: "/Game/Materials")
     *                "recursive" - Whether to search recursively
     * @return JSON array of materials
     */
    TSharedPtr<FJsonObject> HandleListMaterials(const TSharedPtr<FJsonObject>& Params);

    /**
     * Get material info
     * @param Params - Must include:
     *                "path" - Material path
     * @return JSON object with material details
     */
    TSharedPtr<FJsonObject> HandleGetMaterialInfo(const TSharedPtr<FJsonObject>& Params);

    /**
     * Update editable material asset properties such as blend mode and shading model
     * @param Params - Must include:
     *                "material_path" - Path to a base material asset
     *                Optional:
     *                "blend_mode", "shading_model", "material_domain"
     *                "two_sided", "dithered_lod_transition", "use_material_attributes"
     *                "opacity_mask_clip_value", "compile"
     * @return JSON object with the applied material property state
     */
    TSharedPtr<FJsonObject> HandleSetMaterialAssetProperties(const TSharedPtr<FJsonObject>& Params);

    /**
     * Build a reusable material graph scaffold for agent-driven iteration
     * @param Params - Must include:
     *                "material_path" - Path to a base material asset
     *                "scaffold_type" - Preset name (currently "feather_foliage")
     *                Optional:
     *                "base_color", "subsurface_color" - Default vector parameter colors
     *                "coverage_power", "edge_intensity", "opacity_mask_clip_value"
     *                "coverage_function_path", "wpo_function_path"
     *                "layout", "compile", "save"
     * @return JSON object with scaffold aliases and per-step patch results
     */
    TSharedPtr<FJsonObject> HandleCreateMaterialScaffold(const TSharedPtr<FJsonObject>& Params);

    /**
     * Apply material to an actor
     * @param Params - Must include:
     *                "actor_name" - Name of the actor
     *                "material_path" - Path to material
     *                "slot_index" - Optional, material slot index (default: 0)
     * @return JSON response with result
     */
    TSharedPtr<FJsonObject> HandleApplyMaterialToActor(const TSharedPtr<FJsonObject>& Params);

    /**
     * Get the currently opened material in the Material Editor
     * @param Params - No required parameters
     * @return JSON object with opened material path and details
     */
    TSharedPtr<FJsonObject> HandleGetOpenedMaterial(const TSharedPtr<FJsonObject>& Params);

    /**
     * Open a material in the Material Editor
     * @param Params - Must include:
     *                "material_path" - Path to material asset
     *                Optional:
     *                "focus" - Whether to bring window to front (default: true)
     * @return JSON object with opened editor details
     */
    TSharedPtr<FJsonObject> HandleOpenMaterialEditor(const TSharedPtr<FJsonObject>& Params);

    /**
     * Get all expression nodes in a material graph
     * @param Params - Must include:
     *                "material_path" - Path to material (or empty to use currently opened)
     * @return JSON array with all material nodes
     */
    TSharedPtr<FJsonObject> HandleGetMaterialNodes(const TSharedPtr<FJsonObject>& Params);

    /**
     * Get all connections in a material graph
     * @param Params - Must include:
     *                "material_path" - Path to material (or empty to use currently opened)
     * @return JSON array with all connections
     */
    TSharedPtr<FJsonObject> HandleGetMaterialConnections(const TSharedPtr<FJsonObject>& Params);

    /**
     * Create a material expression node in a material graph
     * @param Params - Must include:
     *                "material_path" - Path to material
     *                "node_type" - Expression type (e.g. Constant, Multiply, TextureSample)
     *                Optional:
     *                "node_pos_x", "node_pos_y" - Graph position
     *                "value" - Initial scalar/vector value for supported nodes
     *                "parameter_name" - Parameter name for parameter nodes
     *                "texture_path" - Texture asset path for texture nodes
     *                "material_function_path" - Material function path for function call nodes
     * @return JSON object with created node details
     */
    TSharedPtr<FJsonObject> HandleCreateMaterialNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * Connect two material nodes
     * @param Params - Must include:
     *                "material_path" - Path to material
     *                "from_node" - Source node name
     *                "to_node" - Target node name
     *                Optional:
     *                "from_output" - Source output pin name
     *                "to_input" - Target input pin name
     * @return JSON object with connection details
     */
    TSharedPtr<FJsonObject> HandleConnectMaterialNodes(const TSharedPtr<FJsonObject>& Params);

    /**
     * Connect a material node to a material output property
     * @param Params - Must include:
     *                "material_path" - Path to material
     *                "from_node" - Source node name
     *                "property" - Material property name (e.g. BaseColor, Roughness)
     *                Optional:
     *                "from_output" - Source output pin name
     * @return JSON object with output connection details
     */
    TSharedPtr<FJsonObject> HandleSetMaterialOutput(const TSharedPtr<FJsonObject>& Params);

    /**
     * Delete a material expression node and detach its connections
     * @param Params - Must include:
     *                "material_path" - Path to material
     *                "node_name" - Node name or node guid
     * @return JSON object with deletion details
     */
    TSharedPtr<FJsonObject> HandleDeleteMaterialNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * Disconnect a material node input from another node
     * @param Params - Must include:
     *                "material_path" - Path to material
     *                "to_node" - Target node name or guid
     *                Optional:
     *                "to_input" - Target input pin name
     *                "from_node" - Expected source node name or guid
     * @return JSON object with disconnect details
     */
    TSharedPtr<FJsonObject> HandleDisconnectMaterialNodes(const TSharedPtr<FJsonObject>& Params);

    /**
     * Clear a material output connection
     * @param Params - Must include:
     *                "material_path" - Path to material
     *                "property" - Material property name
     * @return JSON object with cleared output details
     */
    TSharedPtr<FJsonObject> HandleClearMaterialOutput(const TSharedPtr<FJsonObject>& Params);

    /**
     * Move a material node to a new graph position
     * @param Params - Must include:
     *                "material_path" - Path to material
     *                "node_name" - Node name or guid
     *                "node_pos_x", "node_pos_y" - Graph position
     * @return JSON object with updated node location
     */
    TSharedPtr<FJsonObject> HandleMoveMaterialNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * Set an arbitrary reflected property on a material node
     * @param Params - Must include:
     *                "material_path" - Path to material
     *                "node_name" - Node name or guid
     *                "property_name" - Reflected property name
     *                "value" - JSON value to assign
     * @return JSON object with updated property details
     */
    TSharedPtr<FJsonObject> HandleSetMaterialNodeProperty(const TSharedPtr<FJsonObject>& Params);

    /**
     * Layout all material expressions into a readable graph
     * @param Params - Must include:
     *                "material_path" - Path to material
     * @return JSON object with layout result
     */
    TSharedPtr<FJsonObject> HandleLayoutMaterialNodes(const TSharedPtr<FJsonObject>& Params);

    /**
     * Recompile a material after graph changes
     * @param Params - Must include:
     *                "material_path" - Path to material
     *                Optional:
     *                "layout" - Whether to auto-layout expressions before compile
     * @return JSON object with compilation details
     */
    TSharedPtr<FJsonObject> HandleCompileMaterial(const TSharedPtr<FJsonObject>& Params);

    /**
     * Save a material asset
     * @param Params - Must include:
     *                "material_path" - Path to material
     * @return JSON object with save result
     */
    TSharedPtr<FJsonObject> HandleSaveMaterial(const TSharedPtr<FJsonObject>& Params);

    /**
     * Apply a batch of material graph edit operations in one request
     * @param Params - Must include:
     *                "material_path" - Path to material
     *                "operations" - Array of graph edit operations
     * @return JSON object with per-operation results and alias map
     */
    TSharedPtr<FJsonObject> HandleApplyMaterialGraphPatch(const TSharedPtr<FJsonObject>& Params);

    /**
     * Create a material expression node inside a material function
     * @param Params - Must include:
     *                "function_path" - Path to material function
     *                "node_type" - Expression type
     * @return JSON response with created node details
     */
    TSharedPtr<FJsonObject> HandleCreateMaterialFunctionNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * Build a reusable material function scaffold for agent-driven iteration
     * @param Params - Must include:
     *                "function_path" - Path to a material function asset
     *                "scaffold_type" - Preset name ("feather_wpo", "feather_coverage")
     *                Optional:
     *                "sway_frequency", "sway_intensity", "coverage_power"
     *                "layout", "compile", "save"
     * @return JSON object with scaffold aliases and per-step patch results
     */
    TSharedPtr<FJsonObject> HandleCreateMaterialFunctionScaffold(const TSharedPtr<FJsonObject>& Params);

    /**
     * Connect two nodes inside a material function
     * @param Params - Must include:
     *                "function_path" - Path to material function
     *                "from_node" - Source node name or guid
     *                "to_node" - Target node name or guid
     * @return JSON response with connection details
     */
    TSharedPtr<FJsonObject> HandleConnectMaterialFunctionNodes(const TSharedPtr<FJsonObject>& Params);

    /**
     * Disconnect a material function node input from another node
     * @param Params - Must include:
     *                "function_path" - Path to material function
     *                "to_node" - Target node name or guid
     *                Optional:
     *                "to_input" - Target input pin name
     *                "from_node" - Expected source node name or guid
     * @return JSON response with disconnect details
     */
    TSharedPtr<FJsonObject> HandleDisconnectMaterialFunctionNodes(const TSharedPtr<FJsonObject>& Params);

    /**
     * Delete a node from a material function
     * @param Params - Must include:
     *                "function_path" - Path to material function
     *                "node_name" - Node name or guid
     * @return JSON response with deletion details
     */
    TSharedPtr<FJsonObject> HandleDeleteMaterialFunctionNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * Move a material function node to a new graph position
     * @param Params - Must include:
     *                "function_path" - Path to material function
     *                "node_name" - Node name or guid
     *                "node_pos_x", "node_pos_y" - Graph position
     * @return JSON response with updated node position
     */
    TSharedPtr<FJsonObject> HandleMoveMaterialFunctionNode(const TSharedPtr<FJsonObject>& Params);

    /**
     * Layout all material function expressions into a readable graph
     * @param Params - Must include:
     *                "function_path" - Path to material function
     * @return JSON response with layout result
     */
    TSharedPtr<FJsonObject> HandleLayoutMaterialFunctionNodes(const TSharedPtr<FJsonObject>& Params);

    /**
     * Set an arbitrary reflected property on a material function node
     * @param Params - Must include:
     *                "function_path" - Path to material function
     *                "node_name" - Node name or guid
     *                "property_name" - Reflected property name
     *                "value" - JSON value to assign
     * @return JSON response with property update details
     */
    TSharedPtr<FJsonObject> HandleSetMaterialFunctionNodeProperty(const TSharedPtr<FJsonObject>& Params);

    /**
     * Recompile and refresh a material function
     * @param Params - Must include:
     *                "function_path" - Path to material function
     * @return JSON response with refresh details
     */
    TSharedPtr<FJsonObject> HandleCompileMaterialFunction(const TSharedPtr<FJsonObject>& Params);

    /**
     * Save a material function asset
     * @param Params - Must include:
     *                "function_path" - Path to material function
     * @return JSON response with save details
     */
    TSharedPtr<FJsonObject> HandleSaveMaterialFunction(const TSharedPtr<FJsonObject>& Params);

    /**
     * Apply a batch of material function graph edit operations in one request
     * @param Params - Must include:
     *                "function_path" - Path to material function
     *                "operations" - Array of graph edit operations
     * @return JSON object with per-operation results and alias map
     */
    TSharedPtr<FJsonObject> HandleApplyMaterialFunctionPatch(const TSharedPtr<FJsonObject>& Params);

    /**
     * Analyze material and provide detailed explanation
     * @param Params - Must include:
     *                "material_path" - Path to material (or empty to use currently opened)
     * @return JSON object with comprehensive material analysis
     */
    TSharedPtr<FJsonObject> HandleAnalyzeMaterial(const TSharedPtr<FJsonObject>& Params);

    /**
     * Get detailed information about a MaterialFunction
     * @param Params - Must include:
     *                "function_path" - Path to the MaterialFunction asset
     * @return JSON object with function details, inputs, outputs, and internal nodes
     */
    TSharedPtr<FJsonObject> HandleGetMaterialFunctionInfo(const TSharedPtr<FJsonObject>& Params);

    /**
     * Get material flow graph with topological ordering
     * @param Params - Optional:
     *                "material_path" - Path to material (or empty to use currently opened)
     * @return JSON object with structured flow graph data
     */
    TSharedPtr<FJsonObject> HandleGetMaterialFlowGraph(const TSharedPtr<FJsonObject>& Params);

    /**
     * Get complete material graph with all material functions recursively expanded
     * Returns a full hierarchical view with unique node paths for each node
     * @param Params - Optional:
     *                "material_path" - Path to material (or empty to use currently opened)
     *                "max_depth" - Maximum recursion depth (default: 10)
     * @return JSON object with complete expanded graph, node hierarchy, and all connections
     */
    TSharedPtr<FJsonObject> HandleGetMaterialFullGraph(const TSharedPtr<FJsonObject>& Params);

    /**
     * Get material shader compilation info and usage statistics
     * @param Params - Optional:
     *                "material_path" - Path to material (or empty to use currently opened)
     * @return JSON object with shader info, usage flags, and compilation status
     */
    TSharedPtr<FJsonObject> HandleGetMaterialShaderInfo(const TSharedPtr<FJsonObject>& Params);

    /**
     * Get detailed explanation of a material node type
     * Provides human-readable description, typical use cases, and input/output info
     * @param Params - Must include:
     *                "node_type" - The material expression type (e.g., "LinearInterpolate", "Multiply")
     * @return JSON object with detailed node explanation
     */
    TSharedPtr<FJsonObject> HandleExplainMaterialNode(const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * Helper: Recursively collect material function internal data
     * @param MaterialFunction - The material function to process
     * @param ParentPath - Path prefix for node identification
     * @param Depth - Current recursion depth
     * @param MaxDepth - Maximum allowed depth
     * @param OutNodes - Output array for collected nodes
     * @param OutConnections - Output array for collected connections
     * @param ProcessedFunctions - Set of already processed functions to avoid cycles
     */
    void CollectMaterialFunctionGraph(
        UMaterialFunction* MaterialFunction,
        const FString& ParentPath,
        int32 Depth,
        int32 MaxDepth,
        TArray<TSharedPtr<FJsonValue>>& OutNodes,
        TArray<TSharedPtr<FJsonValue>>& OutConnections,
        TSet<FString>& ProcessedFunctions);
};

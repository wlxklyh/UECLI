<#
.SYNOPSIS
    Create 10 XPL-style post-processing effects using pure material nodes via UECLI.
    All effects built with standard UE material expression nodes (no Custom HLSL).
.NOTES
    - to_input="" auto-connects to first input pin
    - SceneTexture Color output is float4, needs ComponentMask(RGB) before float3 operations
    - Use "Fraction" for Desaturation's 2nd input, "" for 1st
#>

param(
    [int]$Port = 31111,
    [int]$Timeout = 30000,
    [string]$ScreenshotDir = "D:/LYH/UE/W0/Saved/Screenshots/PostProcess",
    [string]$MaterialDir = "/Game/Materials/PostProcess",
    [string]$PPVName = "PPV_XPL"
)

function Send-UECLI {
    param([string]$Command, [object]$Params = @{})
    $json = @{ command = $Command; params = $Params } | ConvertTo-Json -Depth 20 -Compress
    try {
        $client = [System.Net.Sockets.TcpClient]::new("127.0.0.1", $Port)
        $stream = $client.GetStream()
        $bytes  = [System.Text.Encoding]::UTF8.GetBytes($json)
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush()
        $buf   = New-Object byte[] 524288
        $stream.ReadTimeout = $Timeout
        $total = ""
        try { while (($n = $stream.Read($buf, 0, $buf.Length)) -gt 0) { $total += [System.Text.Encoding]::UTF8.GetString($buf, 0, $n) } } catch {}
        $client.Close()
        return $total | ConvertFrom-Json
    } catch { Write-Error "UECLI connection failed: $_"; return $null }
}

function New-PPMaterial {
    param([string]$Name, [array]$Operations)
    $matPath = "$MaterialDir/$Name"
    Write-Host "  [$Name]" -ForegroundColor Cyan -NoNewline

    $r = Send-UECLI "create_material" @{ name = $Name; path = $MaterialDir }
    if ($r.status -eq "error" -and $r.error -notmatch "already exists") { Write-Host " CREATE FAIL: $($r.error)" -ForegroundColor Red; return $false }

    Send-UECLI "set_material_asset_properties" @{ material_path=$matPath; material_domain="PostProcess"; blend_mode="Opaque"; shading_model="Unlit" } | Out-Null

    $r = Send-UECLI "apply_material_graph_patch" @{ material_path=$matPath; operations=$Operations; continue_on_error=$false }
    if ($r.status -eq "error") { Write-Host " PATCH FAIL: $($r.error)" -ForegroundColor Red; return $false }

    $r = Send-UECLI "compile_material" @{ material_path = $matPath }
    if ($r.status -eq "error") { Write-Host " COMPILE ERR: $($r.error)" -ForegroundColor Red; return $false }
    if ($r.data.had_compile_error) { Write-Host " SHADER ERR: $($r.data.compile_errors -join '; ')" -ForegroundColor Red; return $false }

    Send-UECLI "save_material" @{ material_path = $matPath } | Out-Null
    Write-Host " OK" -ForegroundColor Green
    return $true
}

# Helper: common ops to create SceneTexture(PPI0) + mask RGB → float3
# Returns alias "scene_rgb" as the float3 color output
function Get-SceneRGBOps {
    param([int]$PosX = -800)
    return @(
        @{ action="create_node"; node_type="SceneTexture"; alias="scene"; pos_x=$PosX; pos_y=0 }
        @{ action="set_node_property"; node_name="scene"; property_name="SceneTextureId"; value=14 }
        @{ action="create_node"; node_type="ComponentMask"; alias="scene_rgb"; pos_x=($PosX+200); pos_y=0 }
        @{ action="set_node_property"; node_name="scene_rgb"; property_name="R"; value=$true }
        @{ action="set_node_property"; node_name="scene_rgb"; property_name="G"; value=$true }
        @{ action="set_node_property"; node_name="scene_rgb"; property_name="B"; value=$true }
        @{ action="set_node_property"; node_name="scene_rgb"; property_name="A"; value=$false }
        @{ action="connect_nodes"; from_node="scene"; from_output="Color"; to_node="scene_rgb"; to_input="" }
    )
}

# =====================================================
# 10 Effects
# =====================================================

$Effects = @()

# ─── 01. Desaturate (Grayscale) ───
$Effects += @{
    Name = "M_PP_Desaturate"; Desc = "Desaturate"
    Ops = (Get-SceneRGBOps -PosX -600) + @(
        @{ action="create_node"; node_type="Desaturation"; alias="desat"; pos_x=-200; pos_y=0 }
        @{ action="create_node"; node_type="Constant"; alias="frac"; pos_x=-400; pos_y=200 }
        @{ action="set_node_property"; node_name="frac"; property_name="R"; value=1.0 }
        @{ action="connect_nodes"; from_node="scene_rgb"; to_node="desat"; to_input="" }
        @{ action="connect_nodes"; from_node="frac"; to_node="desat"; to_input="Fraction" }
        @{ action="set_output"; from_node="desat"; property="EmissiveColor" }
    )
}

# ─── 02. Warm Color Tint ───
$Effects += @{
    Name = "M_PP_WarmTint"; Desc = "WarmTint"
    Ops = (Get-SceneRGBOps -PosX -600) + @(
        @{ action="create_node"; node_type="Constant3Vector"; alias="tint"; pos_x=-400; pos_y=200 }
        @{ action="set_node_property"; node_name="tint"; property_name="Constant"; value=@{R=1.0;G=0.85;B=0.6} }
        @{ action="create_node"; node_type="Multiply"; alias="result"; pos_x=-200; pos_y=0 }
        @{ action="connect_nodes"; from_node="scene_rgb"; to_node="result"; to_input="A" }
        @{ action="connect_nodes"; from_node="tint"; to_node="result"; to_input="B" }
        @{ action="set_output"; from_node="result"; property="EmissiveColor" }
    )
}

# ─── 03. Invert Colors ───
$Effects += @{
    Name = "M_PP_Invert"; Desc = "Invert"
    Ops = (Get-SceneRGBOps -PosX -600) + @(
        @{ action="create_node"; node_type="OneMinus"; alias="inv"; pos_x=-200; pos_y=0 }
        @{ action="connect_nodes"; from_node="scene_rgb"; to_node="inv"; to_input="" }
        @{ action="set_output"; from_node="inv"; property="EmissiveColor" }
    )
}

# ─── 04. Vignette ───
$Effects += @{
    Name = "M_PP_Vignette"; Desc = "Vignette"
    Ops = (Get-SceneRGBOps -PosX -900) + @(
        @{ action="create_node"; node_type="TextureCoordinate"; alias="uv"; pos_x=-900; pos_y=300 }
        @{ action="create_node"; node_type="Constant2Vector"; alias="ctr"; pos_x=-900; pos_y=500 }
        @{ action="set_node_property"; node_name="ctr"; property_name="R"; value=0.5 }
        @{ action="set_node_property"; node_name="ctr"; property_name="G"; value=0.5 }
        @{ action="create_node"; node_type="Subtract"; alias="diff"; pos_x=-700; pos_y=400 }
        @{ action="connect_nodes"; from_node="uv"; to_node="diff"; to_input="A" }
        @{ action="connect_nodes"; from_node="ctr"; to_node="diff"; to_input="B" }
        @{ action="create_node"; node_type="DotProduct"; alias="dot"; pos_x=-500; pos_y=400 }
        @{ action="connect_nodes"; from_node="diff"; to_node="dot"; to_input="A" }
        @{ action="connect_nodes"; from_node="diff"; to_node="dot"; to_input="B" }
        @{ action="create_node"; node_type="Constant"; alias="pow_exp"; pos_x=-500; pos_y=600 }
        @{ action="set_node_property"; node_name="pow_exp"; property_name="R"; value=0.5 }
        @{ action="create_node"; node_type="Power"; alias="pow"; pos_x=-300; pos_y=400 }
        @{ action="connect_nodes"; from_node="dot"; to_node="pow"; to_input="" }
        @{ action="connect_nodes"; from_node="pow_exp"; to_node="pow"; to_input="Exp" }
        @{ action="create_node"; node_type="OneMinus"; alias="vig"; pos_x=-100; pos_y=400 }
        @{ action="connect_nodes"; from_node="pow"; to_node="vig"; to_input="" }
        @{ action="create_node"; node_type="Multiply"; alias="result"; pos_x=100; pos_y=0 }
        @{ action="connect_nodes"; from_node="scene_rgb"; to_node="result"; to_input="A" }
        @{ action="connect_nodes"; from_node="vig"; to_node="result"; to_input="B" }
        @{ action="set_output"; from_node="result"; property="EmissiveColor" }
    )
}

# ─── 05. Sepia Tone ───
$Effects += @{
    Name = "M_PP_Sepia"; Desc = "Sepia"
    Ops = (Get-SceneRGBOps -PosX -800) + @(
        @{ action="create_node"; node_type="Desaturation"; alias="desat"; pos_x=-400; pos_y=0 }
        @{ action="create_node"; node_type="Constant"; alias="frac"; pos_x=-600; pos_y=200 }
        @{ action="set_node_property"; node_name="frac"; property_name="R"; value=0.8 }
        @{ action="connect_nodes"; from_node="scene_rgb"; to_node="desat"; to_input="" }
        @{ action="connect_nodes"; from_node="frac"; to_node="desat"; to_input="Fraction" }
        @{ action="create_node"; node_type="Constant3Vector"; alias="sepia"; pos_x=-400; pos_y=200 }
        @{ action="set_node_property"; node_name="sepia"; property_name="Constant"; value=@{R=1.2;G=1.0;B=0.7} }
        @{ action="create_node"; node_type="Multiply"; alias="result"; pos_x=-100; pos_y=0 }
        @{ action="connect_nodes"; from_node="desat"; to_node="result"; to_input="A" }
        @{ action="connect_nodes"; from_node="sepia"; to_node="result"; to_input="B" }
        @{ action="set_output"; from_node="result"; property="EmissiveColor" }
    )
}

# ─── 06. Brightness / Contrast ───
$Effects += @{
    Name = "M_PP_BrightContrast"; Desc = "BrightContrast"
    Ops = (Get-SceneRGBOps -PosX -900) + @(
        @{ action="create_node"; node_type="Constant"; alias="half"; pos_x=-700; pos_y=200 }
        @{ action="set_node_property"; node_name="half"; property_name="R"; value=0.5 }
        @{ action="create_node"; node_type="Subtract"; alias="sub"; pos_x=-500; pos_y=0 }
        @{ action="connect_nodes"; from_node="scene_rgb"; to_node="sub"; to_input="A" }
        @{ action="connect_nodes"; from_node="half"; to_node="sub"; to_input="B" }
        @{ action="create_node"; node_type="Constant"; alias="contrast"; pos_x=-500; pos_y=200 }
        @{ action="set_node_property"; node_name="contrast"; property_name="R"; value=1.8 }
        @{ action="create_node"; node_type="Multiply"; alias="mul_c"; pos_x=-300; pos_y=0 }
        @{ action="connect_nodes"; from_node="sub"; to_node="mul_c"; to_input="A" }
        @{ action="connect_nodes"; from_node="contrast"; to_node="mul_c"; to_input="B" }
        @{ action="create_node"; node_type="Add"; alias="add_back"; pos_x=-100; pos_y=0 }
        @{ action="connect_nodes"; from_node="mul_c"; to_node="add_back"; to_input="A" }
        @{ action="connect_nodes"; from_node="half"; to_node="add_back"; to_input="B" }
        @{ action="create_node"; node_type="Constant"; alias="bright"; pos_x=-100; pos_y=200 }
        @{ action="set_node_property"; node_name="bright"; property_name="R"; value=1.3 }
        @{ action="create_node"; node_type="Multiply"; alias="result"; pos_x=100; pos_y=0 }
        @{ action="connect_nodes"; from_node="add_back"; to_node="result"; to_input="A" }
        @{ action="connect_nodes"; from_node="bright"; to_node="result"; to_input="B" }
        @{ action="set_output"; from_node="result"; property="EmissiveColor" }
    )
}

# ─── 07. Saturation Boost ───
# Lerp between desaturated and original with alpha > 1 to boost saturation
$Effects += @{
    Name = "M_PP_SatBoost"; Desc = "SatBoost"
    Ops = (Get-SceneRGBOps -PosX -800) + @(
        @{ action="create_node"; node_type="Desaturation"; alias="gray"; pos_x=-400; pos_y=200 }
        @{ action="create_node"; node_type="Constant"; alias="full"; pos_x=-600; pos_y=300 }
        @{ action="set_node_property"; node_name="full"; property_name="R"; value=1.0 }
        @{ action="connect_nodes"; from_node="scene_rgb"; to_node="gray"; to_input="" }
        @{ action="connect_nodes"; from_node="full"; to_node="gray"; to_input="Fraction" }
        # Lerp(gray, scene, 1.8) — alpha > 1 boosts saturation beyond original
        @{ action="create_node"; node_type="Constant"; alias="boost"; pos_x=-200; pos_y=300 }
        @{ action="set_node_property"; node_name="boost"; property_name="R"; value=1.8 }
        @{ action="create_node"; node_type="LinearInterpolate"; alias="result"; pos_x=-100; pos_y=0 }
        @{ action="connect_nodes"; from_node="gray"; to_node="result"; to_input="A" }
        @{ action="connect_nodes"; from_node="scene_rgb"; to_node="result"; to_input="B" }
        @{ action="connect_nodes"; from_node="boost"; to_node="result"; to_input="Alpha" }
        @{ action="set_output"; from_node="result"; property="EmissiveColor" }
    )
}

# ─── 08. RGB Split (Chromatic Aberration) ───
$Effects += @{
    Name = "M_PP_RGBSplit"; Desc = "RGBSplit"
    Ops = @(
        @{ action="create_node"; node_type="TextureCoordinate"; alias="uv"; pos_x=-1200; pos_y=0 }
        # R offset UV
        @{ action="create_node"; node_type="Constant2Vector"; alias="off_r"; pos_x=-1200; pos_y=200 }
        @{ action="set_node_property"; node_name="off_r"; property_name="R"; value=0.005 }
        @{ action="set_node_property"; node_name="off_r"; property_name="G"; value=0.0 }
        @{ action="create_node"; node_type="Add"; alias="uv_r"; pos_x=-900; pos_y=0 }
        @{ action="connect_nodes"; from_node="uv"; to_node="uv_r"; to_input="A" }
        @{ action="connect_nodes"; from_node="off_r"; to_node="uv_r"; to_input="B" }
        # B offset UV
        @{ action="create_node"; node_type="Constant2Vector"; alias="off_b"; pos_x=-1200; pos_y=400 }
        @{ action="set_node_property"; node_name="off_b"; property_name="R"; value=-0.005 }
        @{ action="set_node_property"; node_name="off_b"; property_name="G"; value=0.0 }
        @{ action="create_node"; node_type="Add"; alias="uv_b"; pos_x=-900; pos_y=400 }
        @{ action="connect_nodes"; from_node="uv"; to_node="uv_b"; to_input="A" }
        @{ action="connect_nodes"; from_node="off_b"; to_node="uv_b"; to_input="B" }
        # 3 SceneTexture samples
        @{ action="create_node"; node_type="SceneTexture"; alias="s_r"; pos_x=-600; pos_y=0 }
        @{ action="set_node_property"; node_name="s_r"; property_name="SceneTextureId"; value=14 }
        @{ action="connect_nodes"; from_node="uv_r"; to_node="s_r"; to_input="UVs" }
        @{ action="create_node"; node_type="SceneTexture"; alias="s_g"; pos_x=-600; pos_y=200 }
        @{ action="set_node_property"; node_name="s_g"; property_name="SceneTextureId"; value=14 }
        # G uses default UV (no connection to UVs)
        @{ action="create_node"; node_type="SceneTexture"; alias="s_b"; pos_x=-600; pos_y=400 }
        @{ action="set_node_property"; node_name="s_b"; property_name="SceneTextureId"; value=14 }
        @{ action="connect_nodes"; from_node="uv_b"; to_node="s_b"; to_input="UVs" }
        # Mask channels
        @{ action="create_node"; node_type="ComponentMask"; alias="mr"; pos_x=-300; pos_y=0 }
        @{ action="set_node_property"; node_name="mr"; property_name="R"; value=$true }
        @{ action="set_node_property"; node_name="mr"; property_name="G"; value=$false }
        @{ action="set_node_property"; node_name="mr"; property_name="B"; value=$false }
        @{ action="set_node_property"; node_name="mr"; property_name="A"; value=$false }
        @{ action="connect_nodes"; from_node="s_r"; from_output="Color"; to_node="mr"; to_input="" }
        @{ action="create_node"; node_type="ComponentMask"; alias="mg"; pos_x=-300; pos_y=200 }
        @{ action="set_node_property"; node_name="mg"; property_name="R"; value=$false }
        @{ action="set_node_property"; node_name="mg"; property_name="G"; value=$true }
        @{ action="set_node_property"; node_name="mg"; property_name="B"; value=$false }
        @{ action="set_node_property"; node_name="mg"; property_name="A"; value=$false }
        @{ action="connect_nodes"; from_node="s_g"; from_output="Color"; to_node="mg"; to_input="" }
        @{ action="create_node"; node_type="ComponentMask"; alias="mb"; pos_x=-300; pos_y=400 }
        @{ action="set_node_property"; node_name="mb"; property_name="R"; value=$false }
        @{ action="set_node_property"; node_name="mb"; property_name="G"; value=$false }
        @{ action="set_node_property"; node_name="mb"; property_name="B"; value=$true }
        @{ action="set_node_property"; node_name="mb"; property_name="A"; value=$false }
        @{ action="connect_nodes"; from_node="s_b"; from_output="Color"; to_node="mb"; to_input="" }
        # Combine: Append(R,G) → Append(RG,B)
        @{ action="create_node"; node_type="AppendVector"; alias="rg"; pos_x=0; pos_y=100 }
        @{ action="connect_nodes"; from_node="mr"; to_node="rg"; to_input="A" }
        @{ action="connect_nodes"; from_node="mg"; to_node="rg"; to_input="B" }
        @{ action="create_node"; node_type="AppendVector"; alias="rgb"; pos_x=200; pos_y=200 }
        @{ action="connect_nodes"; from_node="rg"; to_node="rgb"; to_input="A" }
        @{ action="connect_nodes"; from_node="mb"; to_node="rgb"; to_input="B" }
        @{ action="set_output"; from_node="rgb"; property="EmissiveColor" }
    )
}

# ─── 09. Bleach Bypass ───
$Effects += @{
    Name = "M_PP_BleachBypass"; Desc = "BleachBypass"
    Ops = (Get-SceneRGBOps -PosX -900) + @(
        @{ action="create_node"; node_type="Desaturation"; alias="lum"; pos_x=-500; pos_y=200 }
        @{ action="create_node"; node_type="Constant"; alias="one"; pos_x=-700; pos_y=300 }
        @{ action="set_node_property"; node_name="one"; property_name="R"; value=1.0 }
        @{ action="connect_nodes"; from_node="scene_rgb"; to_node="lum"; to_input="" }
        @{ action="connect_nodes"; from_node="one"; to_node="lum"; to_input="Fraction" }
        @{ action="create_node"; node_type="Multiply"; alias="sl"; pos_x=-300; pos_y=100 }
        @{ action="connect_nodes"; from_node="scene_rgb"; to_node="sl"; to_input="A" }
        @{ action="connect_nodes"; from_node="lum"; to_node="sl"; to_input="B" }
        @{ action="create_node"; node_type="Constant"; alias="two"; pos_x=-300; pos_y=300 }
        @{ action="set_node_property"; node_name="two"; property_name="R"; value=2.0 }
        @{ action="create_node"; node_type="Multiply"; alias="boost"; pos_x=-100; pos_y=100 }
        @{ action="connect_nodes"; from_node="sl"; to_node="boost"; to_input="A" }
        @{ action="connect_nodes"; from_node="two"; to_node="boost"; to_input="B" }
        @{ action="create_node"; node_type="Constant"; alias="blend"; pos_x=-100; pos_y=300 }
        @{ action="set_node_property"; node_name="blend"; property_name="R"; value=0.5 }
        @{ action="create_node"; node_type="LinearInterpolate"; alias="result"; pos_x=100; pos_y=0 }
        @{ action="connect_nodes"; from_node="scene_rgb"; to_node="result"; to_input="A" }
        @{ action="connect_nodes"; from_node="boost"; to_node="result"; to_input="B" }
        @{ action="connect_nodes"; from_node="blend"; to_node="result"; to_input="Alpha" }
        @{ action="set_output"; from_node="result"; property="EmissiveColor" }
    )
}

# ─── 10. Old TV (Scanlines + Vignette + Green CRT) ───
$Effects += @{
    Name = "M_PP_OldTV"; Desc = "OldTV"
    Ops = (Get-SceneRGBOps -PosX -1200) + @(
        # Green CRT tint
        @{ action="create_node"; node_type="Constant3Vector"; alias="crt"; pos_x=-1000; pos_y=200 }
        @{ action="set_node_property"; node_name="crt"; property_name="Constant"; value=@{R=0.85;G=1.0;B=0.8} }
        @{ action="create_node"; node_type="Multiply"; alias="tinted"; pos_x=-800; pos_y=0 }
        @{ action="connect_nodes"; from_node="scene_rgb"; to_node="tinted"; to_input="A" }
        @{ action="connect_nodes"; from_node="crt"; to_node="tinted"; to_input="B" }
        # Scanlines via high-frequency UV.y → Sine
        @{ action="create_node"; node_type="TextureCoordinate"; alias="uv_scan"; pos_x=-1200; pos_y=500 }
        @{ action="set_node_property"; node_name="uv_scan"; property_name="UTiling"; value=1.0 }
        @{ action="set_node_property"; node_name="uv_scan"; property_name="VTiling"; value=400.0 }
        @{ action="create_node"; node_type="ComponentMask"; alias="uv_y"; pos_x=-1000; pos_y=500 }
        @{ action="set_node_property"; node_name="uv_y"; property_name="R"; value=$false }
        @{ action="set_node_property"; node_name="uv_y"; property_name="G"; value=$true }
        @{ action="connect_nodes"; from_node="uv_scan"; to_node="uv_y"; to_input="" }
        @{ action="create_node"; node_type="Sine"; alias="sin"; pos_x=-800; pos_y=500 }
        @{ action="connect_nodes"; from_node="uv_y"; to_node="sin"; to_input="" }
        # sin → [0.92, 1.0]: sin*0.04 + 0.96
        @{ action="create_node"; node_type="Constant"; alias="amp"; pos_x=-800; pos_y=650 }
        @{ action="set_node_property"; node_name="amp"; property_name="R"; value=0.04 }
        @{ action="create_node"; node_type="Multiply"; alias="sin_s"; pos_x=-600; pos_y=500 }
        @{ action="connect_nodes"; from_node="sin"; to_node="sin_s"; to_input="A" }
        @{ action="connect_nodes"; from_node="amp"; to_node="sin_s"; to_input="B" }
        @{ action="create_node"; node_type="Constant"; alias="base_v"; pos_x=-600; pos_y=650 }
        @{ action="set_node_property"; node_name="base_v"; property_name="R"; value=0.96 }
        @{ action="create_node"; node_type="Add"; alias="scan_mask"; pos_x=-400; pos_y=500 }
        @{ action="connect_nodes"; from_node="sin_s"; to_node="scan_mask"; to_input="A" }
        @{ action="connect_nodes"; from_node="base_v"; to_node="scan_mask"; to_input="B" }
        # Apply scanlines
        @{ action="create_node"; node_type="Multiply"; alias="with_scan"; pos_x=-200; pos_y=0 }
        @{ action="connect_nodes"; from_node="tinted"; to_node="with_scan"; to_input="A" }
        @{ action="connect_nodes"; from_node="scan_mask"; to_node="with_scan"; to_input="B" }
        # Vignette
        @{ action="create_node"; node_type="TextureCoordinate"; alias="uv_v"; pos_x=-1200; pos_y=800 }
        @{ action="create_node"; node_type="Constant2Vector"; alias="ctr"; pos_x=-1200; pos_y=1000 }
        @{ action="set_node_property"; node_name="ctr"; property_name="R"; value=0.5 }
        @{ action="set_node_property"; node_name="ctr"; property_name="G"; value=0.5 }
        @{ action="create_node"; node_type="Subtract"; alias="vd"; pos_x=-1000; pos_y=800 }
        @{ action="connect_nodes"; from_node="uv_v"; to_node="vd"; to_input="A" }
        @{ action="connect_nodes"; from_node="ctr"; to_node="vd"; to_input="B" }
        @{ action="create_node"; node_type="DotProduct"; alias="vdot"; pos_x=-800; pos_y=800 }
        @{ action="connect_nodes"; from_node="vd"; to_node="vdot"; to_input="A" }
        @{ action="connect_nodes"; from_node="vd"; to_node="vdot"; to_input="B" }
        @{ action="create_node"; node_type="Constant"; alias="vexp"; pos_x=-800; pos_y=950 }
        @{ action="set_node_property"; node_name="vexp"; property_name="R"; value=0.5 }
        @{ action="create_node"; node_type="Power"; alias="vpow"; pos_x=-600; pos_y=800 }
        @{ action="connect_nodes"; from_node="vdot"; to_node="vpow"; to_input="" }
        @{ action="connect_nodes"; from_node="vexp"; to_node="vpow"; to_input="Exp" }
        @{ action="create_node"; node_type="OneMinus"; alias="vmask"; pos_x=-400; pos_y=800 }
        @{ action="connect_nodes"; from_node="vpow"; to_node="vmask"; to_input="" }
        @{ action="create_node"; node_type="Clamp"; alias="vclamp"; pos_x=-200; pos_y=800 }
        @{ action="connect_nodes"; from_node="vmask"; to_node="vclamp"; to_input="" }
        # Final
        @{ action="create_node"; node_type="Multiply"; alias="result"; pos_x=0; pos_y=0 }
        @{ action="connect_nodes"; from_node="with_scan"; to_node="result"; to_input="A" }
        @{ action="connect_nodes"; from_node="vclamp"; to_node="result"; to_input="B" }
        @{ action="set_output"; from_node="result"; property="EmissiveColor" }
    )
}

# =====================================================
# Main
# =====================================================

Write-Host "`n========================================" -ForegroundColor Yellow
Write-Host " XPL Post-Processing (Node Graphs)" -ForegroundColor Yellow
Write-Host "========================================`n" -ForegroundColor Yellow

$ping = Send-UECLI "ping"
if (-not $ping -or $ping.status -ne "success") { Write-Error "UECLI not connected"; exit 1 }
Write-Host "[OK] Connected`n" -ForegroundColor Green

if (-not (Test-Path $ScreenshotDir)) { New-Item -ItemType Directory -Path $ScreenshotDir -Force | Out-Null }

# Save camera
$cam = Send-UECLI "get_viewport_camera"
$savedLoc = $cam.data.location; $savedRot = $cam.data.rotation

# PPV setup
$ppv = Send-UECLI "find_actors_by_name" @{ name = $PPVName }
if (-not $ppv -or $ppv.data.actors.Count -eq 0) {
    Send-UECLI "spawn_actor" @{ type="PostProcessVolume"; name=$PPVName; location=@(0,0,0) } | Out-Null
}
Send-UECLI "set_actor_property" @{ name=$PPVName; property="bUnbound"; value=$true } | Out-Null
Send-UECLI "set_actor_property" @{ name=$PPVName; property="bEnabled"; value=$false } | Out-Null

# Baseline
Write-Host "[BASELINE]" -ForegroundColor Yellow
Start-Sleep -Milliseconds 500
$r = Send-UECLI "take_screenshot" @{ filepath="$ScreenshotDir/PP_00_Baseline.png" }
Write-Host "  $($r.data.width)x$($r.data.height)`n" -ForegroundColor Gray

# Create materials
Write-Host "[CREATING MATERIALS]`n" -ForegroundColor Yellow
$results = @{}
$idx = 0
foreach ($fx in $Effects) {
    $idx++
    Write-Host "  [$idx/10]" -NoNewline
    $results[$fx.Name] = New-PPMaterial -Name $fx.Name -Operations $fx.Ops
}

# Enable PPV and capture
Send-UECLI "set_actor_property" @{ name=$PPVName; property="bEnabled"; value=$true } | Out-Null
Write-Host "`n[SCREENSHOTS]`n" -ForegroundColor Yellow
$idx = 0; $ok = 0
foreach ($fx in $Effects) {
    $idx++
    $matPath = "$MaterialDir/$($fx.Name)"
    $shotPath = "$ScreenshotDir/PP_{0:D2}_{1}.png" -f $idx, $fx.Desc

    if (-not $results[$fx.Name]) { Write-Host "  [$idx] $($fx.Desc) SKIP" -ForegroundColor DarkGray; continue }

    Send-UECLI "set_ppv_material" @{ name=$PPVName; material_path=$matPath; weight=1.0; slot_index=0 } | Out-Null
    Start-Sleep -Milliseconds 1000
    $shot = Send-UECLI "take_screenshot" @{ filepath = $shotPath }
    if ($shot -and $shot.status -eq "success") {
        Write-Host "  [$idx] $($fx.Desc) OK" -ForegroundColor Green; $ok++
    } else {
        Write-Host "  [$idx] $($fx.Desc) FAIL" -ForegroundColor Red
    }
}

# Cleanup
Send-UECLI "set_actor_property" @{ name=$PPVName; property="bEnabled"; value=$false } | Out-Null
if ($savedLoc -and $savedRot) { Send-UECLI "set_viewport_camera" @{ location=$savedLoc; rotation=$savedRot } | Out-Null }

Write-Host "`n========================================" -ForegroundColor Yellow
Write-Host " $ok / 10 effects captured" -ForegroundColor $(if ($ok -ge 8) {"Green"} else {"Yellow"})
Write-Host " $ScreenshotDir" -ForegroundColor Yellow
Write-Host "========================================`n" -ForegroundColor Yellow
Get-ChildItem "$ScreenshotDir/PP_*.png" -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object {
    Write-Host "  $($_.Name)  ($([math]::Round($_.Length/1024,1)) KB)" -ForegroundColor Gray
}

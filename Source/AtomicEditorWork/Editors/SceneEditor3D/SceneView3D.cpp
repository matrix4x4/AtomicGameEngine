// Portions Copyright (c) 2008-2015 the Urho3D project.

// Copyright (c) 2014-2015, THUNDERBEAST GAMES LLC All rights reserved
// Please see LICENSE.md in repository root for license information
// https://github.com/AtomicGameEngine/AtomicGameEngine

#include "AtomicEditor.h"
#include <Atomic/IO/Log.h>
#include <Atomic/Core/CoreEvents.h>
#include <Atomic/Scene/Scene.h>
#include <Atomic/Graphics/Camera.h>

#include <Atomic/Graphics/Graphics.h>
#include <Atomic/Graphics/DebugRenderer.h>
#include <Atomic/Graphics/Viewport.h>
#include <Atomic/Graphics/Octree.h>
#include <Atomic/Graphics/Material.h>

#include <Atomic/Atomic3D/Terrain.h>
#include <Atomic/Atomic3D/Model.h>

#include <Atomic/Input/Input.h>

#include <Atomic/IO/FileSystem.h>
#include <Atomic/Resource/ResourceCache.h>
#include <Atomic/Resource/XMLFile.h>
#include <Atomic/Physics/PhysicsWorld.h>
#include <Atomic/UI/UI.h>
#include <Atomic/UI/UIEvents.h>

#include <Atomic/Resource/ResourceEvents.h>

#include <ToolCore/Assets/Asset.h>
#include <ToolCore/Assets/AssetDatabase.h>

#include "AEEditor.h"
#include "AEEvents.h"

#include "SceneView3D.h"
#include "SceneEditor3D.h"

using namespace ToolCore;

namespace AtomicEditor
{

SceneView3D ::SceneView3D(Context* context, SceneEditor3D *sceneEditor) :
    UISceneView(context),
    yaw_(0.0f),
    pitch_(0.0f),
    mouseLeftDown_(false),
    mouseMoved_(false),
    enabled_(true)
{

    sceneEditor_ = sceneEditor;

    ResourceCache* cache = GetSubsystem<ResourceCache>();

    scene_ = sceneEditor->GetScene();

    debugRenderer_ = scene_->GetComponent<DebugRenderer>();

    if (debugRenderer_.Null())
    {
        debugRenderer_ = scene_->CreateComponent<DebugRenderer>();
    }

    octree_ = scene_->GetComponent<Octree>();

    if (octree_.Null())
    {
        LOGWARNING("Scene without an octree loaded");
        octree_ = scene_->CreateComponent<Octree>();
    }

    cameraNode_ = scene_->CreateChild("Camera");
    cameraNode_->SetTemporary(true);
    camera_ = cameraNode_->CreateComponent<Camera>();

    debugRenderer_ = scene_->GetComponent<DebugRenderer>();
    assert(debugRenderer_.NotNull());
    octree_ = scene_->GetComponent<Octree>();
    assert(octree_.NotNull());

    cameraNode_->SetPosition(Vector3(0, 0, -10));

    SetView(scene_, camera_);
    SetAutoUpdate(false);

    SubscribeToEvent(E_UPDATE, HANDLER(SceneView3D, HandleUpdate));
    SubscribeToEvent(E_EDITORACTIVENODECHANGE, HANDLER(SceneView3D, HandleEditorActiveNodeChange));
    SubscribeToEvent(E_POSTRENDERUPDATE, HANDLER(SceneView3D, HandlePostRenderUpdate));

    SubscribeToEvent(E_MOUSEMOVE, HANDLER(SceneView3D,HandleMouseMove));

    SubscribeToEvent(E_DRAGENTERWIDGET, HANDLER(SceneView3D, HandleDragEnterWidget));
    SubscribeToEvent(E_DRAGEXITWIDGET, HANDLER(SceneView3D, HandleDragExitWidget));
    SubscribeToEvent(E_DRAGENDED, HANDLER(SceneView3D, HandleDragEnded));


    // TODO: generate this event properly
    VariantMap eventData;
    eventData[EditorActiveSceneChange::P_SCENE] = scene_;
    SendEvent(E_EDITORACTIVESCENECHANGE, eventData);

    SetIsFocusable(true);


}

SceneView3D::~SceneView3D()
{

}

void SceneView3D::Enable()
{
    if (enabled_)
        return;

    enabled_ = true;

    SetVisibility(UI_WIDGET_VISIBILITY_VISIBLE);
}

void SceneView3D::Disable()
{
    if (!enabled_)
        return;

    enabled_ = false;

    SetVisibility(UI_WIDGET_VISIBILITY_INVISIBLE);

}

void SceneView3D::MoveCamera(float timeStep)
{
    if (!enabled_)
        return;

    Input* input = GetSubsystem<Input>();

    // Movement speed as world units per second
    float MOVE_SPEED = 20.0f;
    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.2f;

    if (input->GetKeyDown(KEY_LSHIFT) || input->GetKeyDown(KEY_RSHIFT))
        MOVE_SPEED *= 3.0f;

    // Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
    if (input->GetMouseButtonDown(MOUSEB_RIGHT))
    {
        IntVector2 mouseMove = input->GetMouseMove();
        yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
        pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
        pitch_ = Clamp(pitch_, -90.0f, 90.0f);
        // Not working on OSX
        //input->SetMouseMode(MM_RELATIVE);
    }
    else
    {
        // Not working on OSX
        /*
        if (input->GetMouseMode() != MM_ABSOLUTE)
            input->SetMouseMode(MM_ABSOLUTE);
        */
    }


    // Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
    cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

    //Vector3 pos = cameraNode_->GetWorldPosition();
    //Quaternion q = cameraNode_->GetWorldRotation();
    //LOGINFOF("%f %f %f : %f %f %f %f", pos.x_, pos.y_, pos.z_, q.x_, q.y_, q.z_, q.w_ );

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    // Use the Translate() function (default local space) to move relative to the node's orientation.
    if (input->GetKeyDown('W'))
        cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
    if (input->GetKeyDown('S'))
        cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
    if (input->GetKeyDown('A'))
        cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetKeyDown('D'))
        cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);
}

Ray SceneView3D::GetCameraRay()
{
    Ray camRay;

    Input* input = GetSubsystem<Input>();
    IntVector2 cpos = input->GetMousePosition();

    IntRect rect = GetRect();

    if (!rect.Width() || !rect.Height())
        return camRay;

    int x = rect.left_;
    int y = rect.top_;
    GetInternalWidget()->ConvertToRoot(x, y);

    return  camera_->GetScreenRay(float(cpos.x_ - x) / rect.Width(),
                                       float(cpos.y_ - y) / rect.Height());
}

void SceneView3D::DrawNodeDebug(Node* node, DebugRenderer* debug, bool drawNode)
{
    if (drawNode)
        debug->AddNode(node, 1.0, false);

    // Exception for the scene to avoid bringing the editor to its knees: drawing either the whole hierarchy or the subsystem-
    // components can have a large performance hit. Also do not draw terrain child nodes due to their large amount
    // (TerrainPatch component itself draws nothing as debug geometry)
    if (node != scene_ && !node->GetComponent<Terrain>())
    {
        const Vector<SharedPtr<Component> >& components = node->GetComponents();

        for (unsigned j = 0; j < components.Size(); ++j)
            components[j]->DrawDebugGeometry(debug, false);

        // To avoid cluttering the view, do not draw the node axes for child nodes
        for (unsigned k = 0; k < node->GetNumChildren(); ++k)
            DrawNodeDebug(node->GetChild(k), debug, false);
    }
}

bool SceneView3D::MouseInView()
{
    Input* input = GetSubsystem<Input>();
    IntVector2 pos = input->GetMousePosition();

    IntRect rect = GetRect();

    GetInternalWidget()->ConvertToRoot(rect.left_, rect.top_);
    GetInternalWidget()->ConvertToRoot(rect.right_, rect.bottom_);

    return rect.IsInside(pos);

}


void SceneView3D::HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData)
{

    // Visualize the currently selected nodes
    if (selectedNode_.NotNull())
    {
        DrawNodeDebug(selectedNode_, debugRenderer_);

    }

    if (!MouseInView())
        return;

    Input* input = GetSubsystem<Input>();

    mouseLeftDown_ = false;

    if (input->GetMouseButtonPress(MOUSEB_LEFT))
    {
        if (!mouseMoved_ && !sceneEditor_->GetGizmo()->Selected())
        {
            Ray camRay  = GetCameraRay();
            PODVector<RayQueryResult> result;

            RayOctreeQuery query(result, camRay, RAY_TRIANGLE, camera_->GetFarClip(), DRAWABLE_GEOMETRY, 0x7fffffff);
            octree_->RaycastSingle(query);

            if (query.result_.Size())
            {
                const RayQueryResult& r = result[0];

                if (r.drawable_)
                {

                    VariantMap neventData;
                    neventData[EditorActiveNodeChange::P_NODE] = r.drawable_->GetNode();
                    SendEvent(E_EDITORACTIVENODECHANGE, neventData);

                }
            }
        }

        mouseMoved_ = false;

    }
    else if (!input->GetMouseButtonDown(MOUSEB_LEFT))
    {

        Ray camRay  = GetCameraRay();
        PODVector<RayQueryResult> result;

        mouseMoved_ = false;

        /*
        Array<int> pickModeDrawableFlags = {
            DRAWABLE_GEOMETRY,
            DRAWABLE_LIGHT,
            DRAWABLE_ZONE
        };
        */

        RayOctreeQuery query(result, camRay, RAY_TRIANGLE, camera_->GetFarClip(), DRAWABLE_GEOMETRY, 0x7fffffff);
        octree_->RaycastSingle(query);

        if (query.result_.Size())
        {
            const RayQueryResult& r = result[0];

            if (r.drawable_)
            {
                debugRenderer_->AddNode(r.drawable_->GetNode(), 1.0, false);
                r.drawable_->DrawDebugGeometry(debugRenderer_, false);
            }

        }
    }
    else
    {
        mouseLeftDown_ = true;
        if (Abs(input->GetMouseMoveX() > 3 || input->GetMouseMoveY() >  3))
        {
            mouseMoved_ = true;
        }
    }

}

void SceneView3D::SelectNode(Node* node)
{
    selectedNode_ = node;
}

bool SceneView3D::OnEvent(const TBWidgetEvent &ev)
{
    return sceneEditor_->OnEvent(ev);
}


void SceneView3D::HandleUpdate(StringHash eventType, VariantMap& eventData)
{

    // Timestep parameter is same no matter what event is being listened to
    float timeStep = eventData[Update::P_TIMESTEP].GetFloat();

    if (MouseInView())
        MoveCamera(timeStep);

    QueueUpdate();

    if (preloadResourceScene_.NotNull())
    {
        if (preloadResourceScene_->GetAsyncProgress() == 1.0f)
        {
            ResourceCache* cache = GetSubsystem<ResourceCache>();
            XMLFile* xml = cache->GetResource<XMLFile>(dragAssetGUID_);

            if (dragNode_.NotNull())
            {
                dragNode_->LoadXML(xml->GetRoot());
                UpdateDragNode(0, 0);
            }

            preloadResourceScene_ = 0;
            dragAssetGUID_ = "";

        }
    }

}

void SceneView3D::HandleEditorActiveNodeChange(StringHash eventType, VariantMap& eventData)
{
    Node* node = (Node*) (eventData[EditorActiveNodeChange::P_NODE].GetPtr());
    SelectNode(node);
}

void SceneView3D::UpdateDragNode(int mouseX, int mouseY)
{
    if (dragNode_.Null())
        return;

    Ray ray = GetCameraRay();

    Vector3 pos = ray.origin_;
    pos += ray.direction_ * 10;

    dragNode_->SetWorldPosition(pos);

}

void SceneView3D::HandleMouseMove(StringHash eventType, VariantMap& eventData)
{
    if (dragNode_.Null())
        return;

    Input* input = GetSubsystem<Input>();

    if (!input->IsMouseVisible())
        return;

    using namespace MouseMove;

    int x = eventData[P_X].GetInt();
    int y = eventData[P_Y].GetInt();

    UpdateDragNode(x, y);

}

void SceneView3D::HandleDragEnterWidget(StringHash eventType, VariantMap& eventData)
{
    using namespace DragEnterWidget;

    UIWidget* widget = static_cast<UIWidget*>(eventData[P_WIDGET].GetPtr());

    if (widget != this)
        return;

    UIDragObject* dragObject = static_cast<UIDragObject*>(eventData[P_DRAGOBJECT].GetPtr());

    Object* object = dragObject->GetObject();

    if (!object)
        return;

    if (object->GetType() == Asset::GetTypeStatic())
    {
        Asset* asset = (Asset*) object;

        AssetDatabase* db = GetSubsystem<AssetDatabase>();

        const String& importer = asset->GetImporterTypeName();

        if (importer == "ModelImporter")
        {
            dragNode_ = scene_->CreateChild(asset->GetName());

            preloadResourceScene_ = new Scene(context_);

            SharedPtr<File> file(new File(context_, db->GetCachePath() + asset->GetGUID()));

            preloadResourceScene_->LoadAsyncXML(file, LOAD_RESOURCES_ONLY);
            dragAssetGUID_ = asset->GetGUID();

            Input* input = GetSubsystem<Input>();
            IntVector2 pos = input->GetMousePosition();

            UpdateDragNode(pos.x_, pos.y_);

        }

        //LOGINFOF("Dropped %s : %s on SceneView3D", asset->GetPath().CString(), asset->GetGUID().CString());
    }

}

void SceneView3D::HandleDragExitWidget(StringHash eventType, VariantMap& eventData)
{
    if (preloadResourceScene_.NotNull())
    {
        preloadResourceScene_->StopAsyncLoading();
        preloadResourceScene_ = 0;
    }

    if (dragNode_.NotNull())
    {
        // BUG! https://github.com/urho3d/Urho3D/issues/748
        dragNode_->RemoveAllComponents();

        scene_->RemoveChild(dragNode_);
        VariantMap neventData;
        neventData[EditorActiveNodeChange::P_NODE] = (RefCounted*) 0;
        SendEvent(E_EDITORACTIVENODECHANGE, neventData);
    }

    dragAssetGUID_ = "";
    dragNode_ = 0;
}


void SceneView3D::HandleDragEnded(StringHash eventType, VariantMap& eventData)
{
    if (dragNode_.NotNull())
    {
        VariantMap neventData;
        neventData[EditorActiveNodeChange::P_NODE] = dragNode_;
        SendEvent(E_EDITORACTIVENODECHANGE, neventData);
    }
    dragAssetGUID_ = "";
    dragNode_ = 0;
}



}



import ScriptWidget = require("./ScriptWidget");
import Editor = require("../editor/Editor");
import EditorEvents = require("../editor/EditorEvents");
import ProjectFrameMenu = require("./ProjectFrameMenu");
import MenuItemSources = require("./menus/MenuItemSources");

class ProjectFrame extends ScriptWidget {

    folderList: Atomic.UIListView;
    menu: ProjectFrameMenu;
    currentFolder: ToolCore.Asset;
    resourcesID: number;
    assetGUIDToItemID =  {};

    constructor(parent: Atomic.UIWidget) {

        super();

        this.menu = new ProjectFrameMenu();

        this.load("AtomicEditor/editor/ui/projectframe.tb.txt");

        this.gravity = Atomic.UI_GRAVITY_TOP_BOTTOM;

        var projectviewcontainer = parent.getWidget("projectviewcontainer");

        projectviewcontainer.addChild(this);

        var foldercontainer = this.getWidget("foldercontainer");

        var folderList = this.folderList = new Atomic.UIListView();

        folderList.rootList.id = "folderList_";

        this.resourcesID = folderList.addRootItem("Resources", "Folder.icon", "0");

        foldercontainer.addChild(folderList);

        // events
        this.subscribeToEvent("ProjectLoaded", (data) => this.handleProjectLoaded(data));
        this.subscribeToEvent("DragEnded", (data) => this.handleDragEnded(data));

        this.subscribeToEvent("ResourceAdded", (ev: ToolCore.ResourceAddedEvent) => this.handleResourceAdded(ev));

        // this.subscribeToEvent(EditorEvents.ResourceFolderCreated, (ev: EditorEvents.ResourceFolderCreatedEvent) => this.handleResourceFolderCreated(ev));

        // this uses FileWatcher which doesn't catch subfolder creation
        this.subscribeToEvent("FileChanged", (data) => {

            // console.log("File CHANGED! ", data.fileName);

        })

    }

    handleResourceAdded(ev: ToolCore.ResourceAddedEvent) {

        var db = ToolCore.getAssetDatabase();
        var asset = db.getAssetByGUID(ev.guid);

        var parent = asset.parent;
        var folderList = this.folderList;

        // these can be out of order
        if (asset.isFolder()) {

            if (!parent) {

                //  root resource folder
                var id = folderList.addChildItem(this.resourcesID, asset.name, "Folder.icon", asset.guid);
                this.assetGUIDToItemID[asset.guid] = id;

            } else {

                var parentItemID = this.assetGUIDToItemID[parent.guid];
                var id = folderList.addChildItem(parentItemID, asset.name, "Folder.icon", asset.guid);
                this.assetGUIDToItemID[asset.guid] = id;
            }

        }

    }

    /*
    handleResourceFolderCreated(ev: EditorEvents.ResourceFolderCreatedEvent) {

      var db = ToolCore.getAssetDatabase();
      db.scan();

      this.refresh();

      var asset = db.getAssetByPath(ev.path);

      console.log("Asset: ", asset, " : ", ev.path, " : ", ev.navigate);

      if (asset && ev.navigate)
        this.selectPath(asset.path);

    }
    */

    handleWidgetEvent(data: Atomic.UIWidgetEvent): boolean {

        if (data.type == Atomic.UI_EVENT_TYPE_CLICK) {

            var id = data.target.id;

            var system = ToolCore.getToolSystem();
            var project = system.project;


            if (this.menu.handlePopupMenu(data.target, data.refid))
                return true;

            // create
            if (id == "menu create") {

                var src = MenuItemSources.getMenuItemSource("project create items");
                var menu = new Atomic.UIMenuWindow(data.target, "create popup");
                menu.show(src);
                return true;

            }


            var db = ToolCore.getAssetDatabase();

            var fs = Atomic.getFileSystem();

            if (data.target && data.target.id.length) {

                if (id == "folderList_") {

                    var list = <Atomic.UISelectList> data.target;

                    var selectedId = list.selectedItemID;

                    // selectedId == 0 = root "Resources"

                    if (selectedId != "0") {

                        var asset = db.getAssetByGUID(selectedId);

                        if (asset.isFolder)
                            this.refreshContent(asset);
                    }

                    return true;

                }

                var asset = db.getAssetByGUID(id);

                if (asset) {

                    if (asset.isFolder()) {

                        this.folderList.selectItemByID(id);
                        this.refreshContent(asset);

                    } else {

                        this.sendEvent(EditorEvents.EditResource, { "path": asset.path });

                    }

                }

            }

        }

        return false;

    }

    rescan(asset: ToolCore.Asset) {

        var db = ToolCore.getAssetDatabase();
        db.scan();

    }

    selectPath(path: string) {

        var db = ToolCore.getAssetDatabase();

        var asset = db.getAssetByPath(path);

        if (!asset)
            return;

        this.folderList.selectItemByID(asset.guid);

    }

    handleDragEnded(data) {

        // if the drop target is the folderList's root select widget
        var rootList = this.folderList.rootList;
        var hoverID = rootList.hoverItemID;

        if (hoverID == "")
            return;

        var db = ToolCore.getAssetDatabase();
        var asset = db.getAssetByGUID(hoverID);

        if (!asset || !asset.isFolder)
            return;

        var dragObject = <Atomic.UIDragObject> data.dragObject;
        if (dragObject.object && dragObject.object.typeName == "Node") {

            var node = <Atomic.Node> dragObject.object;
            var destFilename = Atomic.addTrailingSlash(asset.path);
            destFilename += node.name + ".prefab";

            var file = new Atomic.File(destFilename, Atomic.FILE_WRITE);
            node.saveXML(file);
            file.close();

            this.rescan(asset);

            return;

        }

        // dropped some files?
        var filenames = dragObject.filenames;

        if (!filenames.length)
            return;

        var fileSystem = Atomic.getFileSystem();


        for (var i in filenames) {

            var srcFilename = filenames[i];

            var pathInfo = Atomic.splitPath(srcFilename);

            var destFilename = Atomic.addTrailingSlash(asset.path);

            destFilename += pathInfo.fileName + pathInfo.ext;

            fileSystem.copy(srcFilename, destFilename);

        }

        this.rescan(asset);

    }

    handleProjectLoaded(data) {


    }

    private refreshContent(folder: ToolCore.Asset) {

        if (this.currentFolder != folder) {

            this.sendEvent(EditorEvents.ContentFolderChanged, { path: folder.path });

        }

        this.currentFolder = folder;

        var db = ToolCore.getAssetDatabase();

        var container: Atomic.UILayout = <Atomic.UILayout> this.getWidget("contentcontainer");
        container.deleteAllChildren();

        var assets = db.getFolderAssets(folder.path);

        for (var i in assets) {

            var asset = assets[i];

            container.addChild(this.createButtonLayout(asset));
        }

    }

    private createButtonLayout(asset: ToolCore.Asset): Atomic.UILayout {

        var system = ToolCore.getToolSystem();
        var project = system.project;
        var fs = Atomic.getFileSystem();

        var pathinfo = Atomic.splitPath(asset.path);

        var bitmapID = "Folder.icon";

        if (fs.fileExists(asset.path)) {
            bitmapID = "FileBitmap";
        }

        if (pathinfo.ext == ".js") {
            if (project.isComponentsDirOrFile(asset.path)) {
                bitmapID = "ComponentBitmap";
            }
            else {
                bitmapID = "JavascriptBitmap";
            }
        }

        var blayout = new Atomic.UILayout();

        blayout.gravity = Atomic.UI_GRAVITY_LEFT;

        var spacer = new Atomic.UIWidget();
        spacer.rect = [0, 0, 8, 8];
        blayout.addChild(spacer);

        var button = new Atomic.UIButton();

        // setup the drag object
        button.dragObject = new Atomic.UIDragObject(asset, asset.name);

        var lp = new Atomic.UILayoutParams;
        lp.height = 20;

        var fd = new Atomic.UIFontDescription();
        fd.id = "Vera";
        fd.size = 11;

        button.gravity = Atomic.UI_GRAVITY_LEFT;

        var image = new Atomic.UISkinImage(bitmapID);
        image.rect = [0, 0, 12, 12];
        image.gravity = Atomic.UI_GRAVITY_RIGHT;
        blayout.addChild(image);

        button.id = asset.guid;
        button.layoutParams = lp;
        button.fontDescription = fd;
        button.text = asset.name;
        button.skinBg = "TBButton.flat";
        blayout.addChild(button);

        return blayout;
    }

}

export = ProjectFrame;

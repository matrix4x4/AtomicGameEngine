import EditorUI = require("../EditorUI");
import ModalWindow = require("./ModalWindow");
import NewProject = require("./NewProject");
import CreateProject = require("./CreateProject");

import EULAWindow = require("../license/EULAWindow");
import ActivationWindow = require("../license/ActivationWindow");
import ActivationSuccessWindow = require("../license/ActivationSuccessWindow");

import ResourceSelection = require("./ResourceSelection");

import UIResourceOps = require("./UIResourceOps");

class ModalOps extends Atomic.ScriptObject {

    constructor() {

        super();

        this.dimmer = new Atomic.UIDimmer();

    }

    showCreateProject(projectTemplateFolder:string) {

      if (this.show()) {

          this.opWindow = new CreateProject(projectTemplateFolder);

      }

    }

    showCreateFolder(resourcePath:string) {

      if (this.show()) {

          this.opWindow = new UIResourceOps.CreateFolder(resourcePath);

      }

    }

    showCreateComponent(resourcePath:string) {

      if (this.show()) {

          this.opWindow = new UIResourceOps.CreateComponent(resourcePath);

      }

    }

    showCreateScene(resourcePath:string) {

      if (this.show()) {

          this.opWindow = new UIResourceOps.CreateScene(resourcePath);

      }

    }

    showResourceDelete(asset:ToolCore.Asset) {

      if (this.show()) {

          this.opWindow = new UIResourceOps.ResourceDelete(asset);

      }

    }

    showResourceSelection(windowText:string, importerType:string, callback: (asset: ToolCore.Asset) => any) {

      if (this.show()) {

          this.opWindow = new ResourceSelection(windowText, importerType, callback);

      }

    }

    showNewProject() {

        if (this.show()) {

            this.opWindow = new NewProject();

        }
    }

    showEULAWindow() {

      if (this.show()) {

          this.opWindow = new EULAWindow();

      }

    }

    showActivationWindow() {

      if (this.show()) {

          this.opWindow = new ActivationWindow();

      }

    }

    showActivationSuccessWindow() {

      if (this.show()) {

          this.opWindow = new ActivationSuccessWindow();

      }

    }

    private show(): boolean {

        if (this.dimmer.parent) {

            console.log("WARNING: attempting to show modal while dimmer is active");
            return false;

        }

        if (this.opWindow) {

            console.log("WARNING: attempting to show modal while another opWindow is active");
            return false;

        }

        var view = EditorUI.getView();
        view.addChild(this.dimmer);

        return true;

    }

    hide() {

        if (this.opWindow) {

            var window = this.opWindow;
            this.opWindow = null;

            if (window.parent)
              window.parent.removeChild(window, false);

            var view = EditorUI.getView();
            view.setFocusRecursive();

        }

        if (this.dimmer.parent) {

            this.dimmer.parent.removeChild(this.dimmer, false);

        }


    }

    dimmer: Atomic.UIDimmer;
    opWindow: ModalWindow;

}

export = ModalOps;

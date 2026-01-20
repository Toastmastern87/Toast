#pragma once

#include "Toast/Project/Project.h"

namespace Toast {

	class ProjectPanel
	{
	public:
		ProjectPanel() = default;
		ProjectPanel(Project* context) { SetContext(context); }
		~ProjectPanel() = default;

		void SetContext(Project* context) { mContext = context; }
		void OnImGuiRender();
	public:
		std::function<void(UUID)> OnOpenSceneRequested;
	private:
		Project* mContext = nullptr;

		UUID mSelectedScene{};
		UUID mRenamingScene{};        // scene currently being renamed (0/invalid when none)
		bool mRenameWantsFocus = false;
		char mRenameBuffer[256] = {};
	};

}
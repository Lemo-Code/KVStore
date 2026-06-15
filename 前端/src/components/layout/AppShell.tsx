import { AIAssistantPanel } from '@/components/ai/AIAssistantPanel'
import { ChatPanel } from '@/components/chat/ChatPanel'
import { CommandConsole } from '@/components/database/CommandConsole'
import { KeyDetailPanel } from '@/components/database/KeyDetailPanel'
import { LearningPanel } from '@/components/learning/LearningPanel'
import { ConnectionSidebar } from '@/components/layout/ConnectionSidebar'
import { MonitorPanel } from '@/components/layout/MonitorPanel'
import { StatusBar } from '@/components/layout/StatusBar'
import { TabBar } from '@/components/layout/TabBar'
import { TopBar } from '@/components/layout/TopBar'
import { useAppStore } from '@/stores/appStore'
import { Panel, PanelGroup, PanelResizeHandle } from 'react-resizable-panels'

function RightPanelContent() {
  const { rightPanel } = useAppStore()

  switch (rightPanel) {
    case 'ai':
      return <AIAssistantPanel />
    case 'chat':
      return <ChatPanel />
    case 'learning':
      return <LearningPanel />
  }
}

function MainWorkspace() {
  const { activeTab } = useAppStore()

  return (
    <div className="flex h-full flex-col bg-surface-2">
      <TabBar />
      <div className="flex-1 min-h-0">
        {activeTab === 'tab-console' ? <CommandConsole /> : <KeyDetailPanel />}
      </div>
    </div>
  )
}

export function AppShell() {
  return (
    <div className="flex h-full flex-col">
      <TopBar />

      <div className="flex flex-1 min-h-0">
        <PanelGroup direction="horizontal" autoSaveId="redis-lab-layout">
          {/* Left: Connection tree */}
          <Panel defaultSize={18} minSize={14} maxSize={30}>
            <ConnectionSidebar />
          </Panel>

          <PanelResizeHandle />

          {/* Center: Main workspace */}
          <Panel defaultSize={52} minSize={35}>
            <PanelGroup direction="vertical" autoSaveId="redis-lab-center">
              <Panel defaultSize={75} minSize={40}>
                <MainWorkspace />
              </Panel>
              <PanelResizeHandle />
              <Panel defaultSize={25} minSize={0} collapsible>
                <MonitorPanel />
              </Panel>
            </PanelGroup>
          </Panel>

          <PanelResizeHandle />

          {/* Right: AI / Chat / Learning */}
          <Panel defaultSize={30} minSize={22} maxSize={45}>
            <div className="h-full border-l border-border-subtle bg-surface-1">
              <RightPanelContent />
            </div>
          </Panel>
        </PanelGroup>
      </div>

      <StatusBar />
    </div>
  )
}

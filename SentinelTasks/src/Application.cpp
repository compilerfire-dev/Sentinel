#include "Application.hpp"

#include <algorithm>
#include <array>
#include <clocale>
#include <cctype>
#include <ncurses.h>
#include <sstream>
#include <string_view>

namespace {
struct CommandDefinition { std::string_view name; std::string_view description; };
constexpr std::array<CommandDefinition, 9> Commands{{
    {"addFolder", "add a folder node"}, {"addTask", "add an individual task"},
    {"remove", "remove a node and its subtree"}, {"setDescription", "set a node description"},
    {"manualSelect", "select nodes with arrows/mouse"}, {"select", "select a node by ID"},
    {"commands", "show all commands"}, {"list", "return to the tree view"}, {"quit", "exit SentinelTasks"}
}};
constexpr std::size_t MaxSuggestions = 6;
constexpr std::size_t MaxDropdownRows = 8;
const std::vector<std::string> CommandHelp{
    "addFolder <id> <parent|root> <name>     Add a folder/category node",
    "addTask <id> <parent|root> <name>       Add an individual task node",
    "remove <id>                              Remove a node and descendants",
    "setDescription <id> <description>        Set text shown in the right pane",
    "manualSelect [id]                        Enter arrow/mouse selection mode",
    "select <id>                              Select a node by ID",
    "commands                                 Show this command list",
    "list                                     Return to tree view",
    "quit                                     Exit SentinelTasks",
    "", "Enter an argument-taking command by itself to open its argument window."
};
struct DialogRect { int top{}, left{}, height{}, width{}; };
bool IsUtf8Continuation(unsigned char c) { return (c & 0xC0U) == 0x80U; }
std::string Trim(std::string s) {
    const auto first = s.find_first_not_of(" \t"); if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(" \t") - first + 1);
}
void DrawClipped(int row, int col, int width, const std::string& value) {
    if (width > 0 && col >= 0) mvaddnstr(row, col, value.c_str(), width);
}
std::vector<std::string> WrapText(const std::string& text, int width) {
    std::vector<std::string> lines; if (width <= 0) return lines;
    std::istringstream in(text); std::string word, line;
    while (in >> word) {
        if (line.empty()) line = word;
        else if (static_cast<int>(line.size() + word.size() + 1) <= width) line += " " + word;
        else { lines.push_back(line); line = word; }
    }
    if (!line.empty()) lines.push_back(line); return lines;
}
DialogRect CalculateDialogRect(std::size_t fields) {
    int h, w; getmaxyx(stdscr, h, w); DialogRect r;
    r.width = std::clamp(w * 3 / 5, 54, std::max(54, w - 4));
    r.height = std::clamp(9 + static_cast<int>(fields) * 2, 11, std::max(11, h - 4));
    r.left = std::max(0, (w - r.width) / 2); r.top = std::max(0, (h - r.height) / 2); return r;
}
int DialogFieldRow(const DialogRect& r, std::size_t i) { return r.top + 4 + static_cast<int>(i) * 2; }
}

int Application::Run() {
    std::setlocale(LC_ALL, ""); initscr(); cbreak(); noecho(); keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS, nullptr); timeout(100); curs_set(1);
    while (running_) { HandleInput(); Render(); }
    endwin(); return 0;
}

void Application::HandleInput() {
    const int key = getch(); if (key == ERR) return;
    if (commandDialog_) {
        if (key == KEY_MOUSE) { MEVENT e{}; if (getmouse(&e) == OK) HandleCommandDialogMouse(e.x, e.y); }
        else HandleCommandDialogInput(key); return;
    }
    if (manualSelect_) {
        if (key == KEY_MOUSE) HandleMouse(); else if (key == KEY_UP) MoveManualSelection(-1);
        else if (key == KEY_DOWN) MoveManualSelection(1); else if (key == KEY_LEFT) SelectParent();
        else if (key == KEY_RIGHT) SelectFirstChild(); else if (key == '\n' || key == KEY_ENTER || key == 27) LeaveManualSelect();
        return;
    }
    const auto suggestions = BuildSuggestions();
    if (key == KEY_MOUSE) { HandleMouse(); return; }
    if (key == '\t') { AcceptSuggestion(suggestions); ResetHistoryNavigation(); return; }
    if (key == KEY_LEFT) { cursorPosition_ = PreviousUtf8Boundary(commandBuffer_, cursorPosition_); return; }
    if (key == KEY_RIGHT) { cursorPosition_ = NextUtf8Boundary(commandBuffer_, cursorPosition_); return; }
    if (key == KEY_UP) {
        if (navigatingSuggestions_ && !suggestions.empty()) selectedSuggestion_ = selectedSuggestion_ == 0 ? suggestions.size()-1 : selectedSuggestion_-1;
        else RecallPreviousCommand(); return;
    }
    if (key == KEY_DOWN) {
        if (navigatingSuggestions_ && !suggestions.empty()) selectedSuggestion_ = (selectedSuggestion_+1)%suggestions.size();
        else if (historyIndex_) RecallNextCommand(); else if (!suggestions.empty()) { navigatingSuggestions_=true; selectedSuggestion_=0; }
        return;
    }
    if (key == '\n' || key == KEY_ENTER) {
        if (navigatingSuggestions_ && !suggestions.empty()) { AcceptSuggestion(suggestions); return; }
        const std::string command = Trim(commandBuffer_); if (!command.empty()) { AddCommandToHistory(command); ExecuteCommand(command); }
        commandBuffer_.clear(); cursorPosition_=0; ResetSuggestionNavigation(); return;
    }
    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (cursorPosition_ > 0) { auto p=PreviousUtf8Boundary(commandBuffer_,cursorPosition_); commandBuffer_.erase(p,cursorPosition_-p); cursorPosition_=p; }
        ResetHistoryNavigation(); ResetSuggestionNavigation(); return;
    }
    if (key >= 32 && key <= 255) {
        commandBuffer_.insert(commandBuffer_.begin()+static_cast<std::ptrdiff_t>(cursorPosition_), static_cast<char>(key)); ++cursorPosition_;
        ResetHistoryNavigation(); ResetSuggestionNavigation();
    }
}

void Application::HandleMouse() {
    MEVENT e{}; if (getmouse(&e) != OK) return;
    if ((e.bstate & BUTTON1_CLICKED)==0 && (e.bstate & BUTTON1_PRESSED)==0) return;
    int h,w; getmaxyx(stdscr,h,w); (void)w;
    if (manualSelect_) { int i=e.y-2; if (i>=0 && i<static_cast<int>(visibleRowIds_.size())) selectedId_=visibleRowIds_[i]; return; }
    if (e.y==h-1) { cursorPosition_=std::min(commandBuffer_.size(),static_cast<std::size_t>(std::max(0,e.x-2))); return; }
    const auto s=BuildSuggestions(); if (s.empty()) return; int start=std::max(2,h-2-static_cast<int>(s.size()));
    if (e.y>=start && e.y<start+static_cast<int>(s.size())) { selectedSuggestion_=e.y-start; AcceptSuggestion(s); }
}

void Application::ExecuteCommand(const std::string& line) {
    const auto t=Tokenize(line); if (t.empty()) return; const std::string& c=t[0]; infoLines_.clear();
    if (t.size()==1 && OpenCommandDialog(c)) return;
    if (c=="quit" || c=="exit") { running_=false; return; }
    if (c=="commands") { infoLines_=CommandHelp; status_="Available SentinelTasks commands."; return; }
    if (c=="list") { status_="Tree view."; return; }
    if (c=="manualSelect") { if (t.size()>2) status_="Usage: manualSelect [id]"; else EnterManualSelect(t.size()==2?std::optional<std::string>{t[1]}:std::nullopt); return; }
    if (c=="select") { if (t.size()!=2 || !tree_.GetNode(t[1])) status_="Usage: select <existing-id>"; else { selectedId_=t[1]; status_="Selected: "+selectedId_; } return; }
    if (c=="remove") {
        if (t.size()!=2) { status_="Usage: remove <id>"; return; } std::string error;
        if (!tree_.RemoveNode(t[1],error)) status_=error; else { if (!tree_.GetNode(selectedId_)) selectedId_.clear(); EnsureSelection(); status_="Removed: "+t[1]; } return;
    }
    if (c=="setDescription") {
        if (t.size()<3) { status_="Usage: setDescription <id> <description>"; return; } std::string error;
        status_=tree_.SetDescription(t[1],JoinTokens(t,2),error)?"Description updated: "+t[1]:error; return;
    }
    if (c=="addFolder" || c=="addTask") {
        if (t.size()<4) { status_="Usage: "+c+" <id> <parent|root> <name>"; return; }
        std::string error; NodeKind kind=c=="addFolder"?NodeKind::Folder:NodeKind::Task;
        if (!tree_.AddNode(kind,t[1],t[2],JoinTokens(t,3),error)) status_=error;
        else { selectedId_=t[1]; status_=std::string(kind==NodeKind::Folder?"Folder added: ":"Task added: ")+t[1]; } return;
    }
    status_="Unknown command: "+c+". Type 'commands'.";
}

void Application::Render() {
    erase(); EnsureSelection(); RenderHeader(); RenderTree(); RenderDescriptionPane();
    if (!manualSelect_ && !commandDialog_) RenderSuggestions(BuildSuggestions());
    RenderStatus(); RenderCommandLine(); if (commandDialog_) RenderCommandDialog(); refresh();
}
void Application::RenderHeader() {
    int h,w; getmaxyx(stdscr,h,w); (void)h; std::string title=commandDialog_?"SentinelTasks | COMMAND ARGUMENT WINDOW":manualSelect_?"SentinelTasks | MANUAL SELECT":"SentinelTasks | Tree Task Planner";
    DrawClipped(0,0,std::max(0,w-1),title); if(w>1)mvhline(1,0,ACS_HLINE,w-1);
}
void Application::RenderTree() {
    int h,w; getmaxyx(stdscr,h,w); int divider=std::clamp(w*64/100,40,std::max(40,w-30)); int treeWidth=std::max(1,divider-2); int last=std::max(2,h-3);
    visibleRowIds_.clear(); if(!infoLines_.empty()){int row=2;for(const auto& l:infoLines_){if(row>=last)break;DrawClipped(row++,0,treeWidth,l);}return;}
    auto visible=tree_.Flatten(); int row=2; for(const auto& e:visible){if(row>=last||!e.node)break; const auto& n=*e.node; bool selected=n.id==selectedId_;
        std::string kind=n.kind==NodeKind::Folder?"[F] ":"[T] "; std::string text=e.connectorPrefix+kind+n.name+"  {"+n.id+"}";
        if(selected)attron(A_REVERSE);DrawClipped(row,0,treeWidth,text);if(selected)attroff(A_REVERSE);visibleRowIds_.push_back(n.id);++row;}
    if(visible.empty())DrawClipped(3,0,treeWidth,"No nodes yet. Use addFolder or addTask.");
}
void Application::RenderDescriptionPane() {
    int h,w;getmaxyx(stdscr,h,w);int d=std::clamp(w*64/100,40,std::max(40,w-30));if(d>=w-2)return;for(int r=2;r<h-2;++r)mvaddch(r,d,ACS_VLINE);
    int col=d+2,pw=std::max(1,w-col-1);DrawClipped(2,col,pw,"Description");mvhline(3,col,ACS_HLINE,pw);const TaskNode* n=tree_.GetNode(selectedId_);
    if(!n){DrawClipped(5,col,pw,"No task selected.");return;}DrawClipped(5,col,pw,"ID: "+n->id);DrawClipped(6,col,pw,"Type: "+std::string(n->kind==NodeKind::Folder?"folder":"task"));DrawClipped(8,col,pw,n->name);
    auto lines=WrapText(n->description.empty()?"(No description)":n->description,pw);int row=10;for(const auto& l:lines){if(row>=h-3)break;DrawClipped(row++,col,pw,l);}
}
void Application::RenderSuggestions(const std::vector<Suggestion>& s) {
    if(s.empty())return;int h,w;getmaxyx(stdscr,h,w);int start=std::max(2,h-2-static_cast<int>(s.size()));for(std::size_t i=0;i<s.size();++i){if(i==selectedSuggestion_)attron(A_REVERSE);DrawClipped(start+i,0,w-1,(i==selectedSuggestion_?"> ":"  ")+s[i].label);if(i==selectedSuggestion_)attroff(A_REVERSE);}
}
void Application::RenderStatus() { int h,w;getmaxyx(stdscr,h,w);std::string s=status_;if(commandDialog_)s+=" | Tab fields | Enter choose | F2 submit | Esc cancel";else if(manualSelect_)s+=" | Up/Down select | Left parent | Right child | Enter/Esc finish";DrawClipped(h-2,0,w-1,s); }
void Application::RenderCommandLine() { int h,w;getmaxyx(stdscr,h,w);move(h-1,0);clrtoeol();if(commandDialog_){curs_set(0);DrawClipped(h-1,0,w-1,"> [argument window: "+commandDialog_->command+"]");return;}if(manualSelect_){curs_set(0);DrawClipped(h-1,0,w-1,"> [manualSelect] "+selectedId_);return;}curs_set(1);DrawClipped(h-1,0,2,"> ");DrawClipped(h-1,2,w-3,commandBuffer_);move(h-1,std::min(w-1,static_cast<int>(cursorPosition_)+2)); }

void Application::RenderCommandDialog() {
    if(!commandDialog_)return;auto& d=*commandDialog_;auto r=CalculateDialogRect(d.fields.size());
    for(int y=r.top;y<r.top+r.height;++y){move(y,r.left);for(int x=0;x<r.width;++x)addch(' ');}mvhline(r.top,r.left+1,ACS_HLINE,r.width-2);mvhline(r.top+r.height-1,r.left+1,ACS_HLINE,r.width-2);mvvline(r.top+1,r.left,ACS_VLINE,r.height-2);mvvline(r.top+1,r.left+r.width-1,ACS_VLINE,r.height-2);
    mvaddch(r.top,r.left,ACS_ULCORNER);mvaddch(r.top,r.left+r.width-1,ACS_URCORNER);mvaddch(r.top+r.height-1,r.left,ACS_LLCORNER);mvaddch(r.top+r.height-1,r.left+r.width-1,ACS_LRCORNER);
    DrawClipped(r.top+1,r.left+3,r.width-6,d.title);DrawClipped(r.top+2,r.left+3,r.width-6,"Fill arguments manually. CLI arguments remain supported.");
    int labelWidth=std::min(18,std::max(10,r.width/4)), inputCol=r.left+3+labelWidth,inputWidth=std::max(10,r.width-labelWidth-7);
    for(std::size_t i=0;i<d.fields.size();++i){auto& f=d.fields[i];int row=DialogFieldRow(r,i);DrawClipped(row,r.left+3,labelWidth-1,f.label+":");if(d.focusedControl==i)attron(A_REVERSE);std::string value=f.value.empty()&&f.kind==DialogFieldKind::DropList?"(no options)":f.value;std::string display="[ "+value+(f.kind==DialogFieldKind::DropList?"  v":"")+" ]";DrawClipped(row,inputCol,inputWidth,display);if(d.focusedControl==i)attroff(A_REVERSE);}
    std::size_t submit=d.fields.size(),cancel=submit+1;int br=r.top+r.height-3,sc=r.left+r.width/2-14,cc=r.left+r.width/2+3;if(d.focusedControl==submit)attron(A_REVERSE);DrawClipped(br,sc,12,"[ Submit ]");if(d.focusedControl==submit)attroff(A_REVERSE);if(d.focusedControl==cancel)attron(A_REVERSE);DrawClipped(br,cc,12,"[ Cancel ]");if(d.focusedControl==cancel)attroff(A_REVERSE);DrawClipped(r.top+r.height-2,r.left+3,r.width-6,d.validationMessage);
    if(d.focusedControl<d.fields.size()&&d.fields[d.focusedControl].kind==DialogFieldKind::TextInput){curs_set(1);move(DialogFieldRow(r,d.focusedControl),std::min(inputCol+inputWidth-2,inputCol+2+static_cast<int>(d.fields[d.focusedControl].cursor)));}else curs_set(0);
}

std::vector<Application::Suggestion> Application::BuildSuggestions() const {
    std::vector<Suggestion> out;if(commandBuffer_.empty()||commandBuffer_.find_first_of(" \t")!=std::string::npos)return out;std::string q=commandBuffer_;std::transform(q.begin(),q.end(),q.begin(),[](unsigned char c){return std::tolower(c);});
    for(const auto& c:Commands){std::string n(c.name),lower=n;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char x){return std::tolower(x);});if(lower.find(q)!=std::string::npos)out.push_back({n+"  -  "+std::string(c.description),n+" "});if(out.size()==MaxSuggestions)break;}return out;
}
void Application::AcceptSuggestion(const std::vector<Suggestion>& s){if(s.empty())return;selectedSuggestion_=std::min(selectedSuggestion_,s.size()-1);commandBuffer_=s[selectedSuggestion_].replacement;cursorPosition_=commandBuffer_.size();ResetSuggestionNavigation();}
void Application::ResetSuggestionNavigation(){selectedSuggestion_=0;navigatingSuggestions_=false;}
void Application::AddCommandToHistory(const std::string& c){if(!c.empty()&&(commandHistory_.empty()||commandHistory_.back()!=c))commandHistory_.push_back(c);ResetHistoryNavigation();}
void Application::RecallPreviousCommand(){if(commandHistory_.empty())return;if(!historyIndex_){commandBeforeHistory_=commandBuffer_;historyIndex_=commandHistory_.size()-1;}else if(*historyIndex_>0)--*historyIndex_;commandBuffer_=commandHistory_[*historyIndex_];cursorPosition_=commandBuffer_.size();}
void Application::RecallNextCommand(){if(!historyIndex_)return;if(*historyIndex_+1<commandHistory_.size()){++*historyIndex_;commandBuffer_=commandHistory_[*historyIndex_];}else{commandBuffer_=commandBeforeHistory_;ResetHistoryNavigation();}cursorPosition_=commandBuffer_.size();}
void Application::ResetHistoryNavigation(){historyIndex_.reset();commandBeforeHistory_.clear();}
void Application::EnterManualSelect(const std::optional<std::string>& id){if(tree_.Empty()){status_="Cannot enter manualSelect: tree is empty.";return;}if(id){if(!tree_.GetNode(*id)){status_="Node ID does not exist: "+*id;return;}selectedId_=*id;}EnsureSelection();manualSelect_=true;status_="Manual selection mode.";}
void Application::LeaveManualSelect(){manualSelect_=false;curs_set(1);}
void Application::MoveManualSelection(int delta){auto v=tree_.Flatten();if(v.empty())return;std::size_t i=0;for(std::size_t j=0;j<v.size();++j)if(v[j].node&&v[j].node->id==selectedId_){i=j;break;}i=delta<0?(i==0?v.size()-1:i-1):(i+1)%v.size();selectedId_=v[i].node->id;}
void Application::SelectParent(){if(auto p=tree_.ParentOf(selectedId_))selectedId_=*p;}
void Application::SelectFirstChild(){if(auto c=tree_.FirstChildOf(selectedId_))selectedId_=*c;}
void Application::EnsureSelection(){if(!selectedId_.empty()&&tree_.GetNode(selectedId_))return;auto v=tree_.Flatten();selectedId_=v.empty()?std::string{}:v.front().node->id;}

bool Application::OpenCommandDialog(const std::string& command){CommandDialog d;d.command=command;d.title="Command: "+command;auto text=[](std::string label){DialogField f;f.label=std::move(label);return f;};auto drop=[&](std::string label,std::vector<std::string> options,std::string preferred={}){DialogField f;f.label=std::move(label);f.kind=DialogFieldKind::DropList;f.options=std::move(options);f.selectedOption=FindOptionIndex(f.options,preferred).value_or(0);if(!f.options.empty())f.value=f.options[f.selectedOption];return f;};
    if(command=="addFolder"||command=="addTask"){d.fields.push_back(text("ID"));std::string parent="root";if(const auto* n=tree_.GetNode(selectedId_)){if(n->kind==NodeKind::Folder)parent=n->id;else if(!n->parentId.empty())parent=n->parentId;}d.fields.push_back(drop("Parent",NodeIdOptions(true,true),parent));d.fields.push_back(text("Name"));}
    else if(command=="setDescription"){d.fields.push_back(drop("Node",NodeIdOptions(false,false),selectedId_));DialogField f=text("Description");if(const auto* n=tree_.GetNode(selectedId_))f.value=n->description;f.cursor=f.value.size();d.fields.push_back(std::move(f));}
    else if(command=="remove"||command=="select"||command=="manualSelect")d.fields.push_back(drop("Node",NodeIdOptions(false,false),selectedId_));else return false;
    commandDialog_=std::move(d);status_="Argument window opened for: "+command;return true;}
void Application::CloseCommandDialog(){commandDialog_.reset();curs_set(1);}
void Application::HandleCommandDialogInput(int key){if(!commandDialog_)return;auto& d=*commandDialog_;if(key==27){CloseCommandDialog();return;}if(key==KEY_F(2)){SubmitCommandDialog();return;}if(key=='\t'){MoveDialogFocus(1);return;}std::size_t submit=d.fields.size(),cancel=submit+1;if(d.focusedControl==submit){if(key=='\n'||key==KEY_ENTER||key==' ')SubmitCommandDialog();return;}if(d.focusedControl==cancel){if(key=='\n'||key==KEY_ENTER||key==' ')CloseCommandDialog();return;}if(d.focusedControl>=d.fields.size())return;auto& f=d.fields[d.focusedControl];if(f.kind==DialogFieldKind::DropList){if(f.options.empty())return;if(key==KEY_UP)f.selectedOption=f.selectedOption==0?f.options.size()-1:f.selectedOption-1;else if(key==KEY_DOWN)f.selectedOption=(f.selectedOption+1)%f.options.size();else if(key=='\n'||key==KEY_ENTER)MoveDialogFocus(1);f.value=f.options[f.selectedOption];return;}if(key==KEY_LEFT)f.cursor=PreviousUtf8Boundary(f.value,f.cursor);else if(key==KEY_RIGHT)f.cursor=NextUtf8Boundary(f.value,f.cursor);else if(key==KEY_BACKSPACE||key==127||key==8){if(f.cursor>0){auto p=PreviousUtf8Boundary(f.value,f.cursor);f.value.erase(p,f.cursor-p);f.cursor=p;}}else if(key=='\n'||key==KEY_ENTER)MoveDialogFocus(1);else if(key>=32&&key<=255){f.value.insert(f.value.begin()+static_cast<std::ptrdiff_t>(f.cursor),static_cast<char>(key));++f.cursor;}}
void Application::HandleCommandDialogMouse(int x,int y){if(!commandDialog_)return;auto& d=*commandDialog_;auto r=CalculateDialogRect(d.fields.size());int lw=std::min(18,std::max(10,r.width/4)),ic=r.left+3+lw,iw=std::max(10,r.width-lw-7);for(std::size_t i=0;i<d.fields.size();++i)if(y==DialogFieldRow(r,i)&&x>=ic&&x<ic+iw){d.focusedControl=i;return;}int br=r.top+r.height-3,sc=r.left+r.width/2-14,cc=r.left+r.width/2+3;if(y==br&&x>=sc&&x<sc+12)SubmitCommandDialog();else if(y==br&&x>=cc&&x<cc+12)CloseCommandDialog();}
void Application::MoveDialogFocus(int delta){if(!commandDialog_)return;auto& d=*commandDialog_;std::size_t count=d.fields.size()+2;d.focusedControl=delta<0?(d.focusedControl==0?count-1:d.focusedControl-1):(d.focusedControl+1)%count;}
void Application::OpenFocusedDropList(){if(commandDialog_&&commandDialog_->focusedControl<commandDialog_->fields.size())commandDialog_->fields[commandDialog_->focusedControl].dropdownOpen=true;}
void Application::CloseFocusedDropList(bool accept){if(!commandDialog_||commandDialog_->focusedControl>=commandDialog_->fields.size())return;auto& f=commandDialog_->fields[commandDialog_->focusedControl];if(accept&&!f.options.empty())f.value=f.options[f.selectedOption];f.dropdownOpen=false;}
bool Application::SubmitCommandDialog(){if(!commandDialog_)return false;for(const auto& f:commandDialog_->fields)if(f.value.empty()){commandDialog_->validationMessage="Required field is empty: "+f.label;return false;}std::string line=BuildDialogCommand();CloseCommandDialog();AddCommandToHistory(line);ExecuteCommand(line);return true;}
std::string Application::BuildDialogCommand() const{if(!commandDialog_)return{};std::string r=commandDialog_->command;for(const auto& f:commandDialog_->fields)r+=" "+QuoteArgument(f.value);return r;}
std::vector<std::string> Application::NodeIdOptions(bool foldersOnly,bool includeRoot) const{std::vector<std::string> o;if(includeRoot)o.push_back("root");for(const auto& v:tree_.Flatten())if(v.node&&(!foldersOnly||v.node->kind==NodeKind::Folder))o.push_back(v.node->id);return o;}
std::optional<std::size_t> Application::FindOptionIndex(const std::vector<std::string>& o,const std::string& v) const{auto it=std::find(o.begin(),o.end(),v);return it==o.end()?std::nullopt:std::optional<std::size_t>{static_cast<std::size_t>(std::distance(o.begin(),it))};}
std::string Application::QuoteArgument(const std::string& v){std::string r="\"";for(char c:v){if(c=='\\'||c=='\"')r+='\\';r+=c;}return r+'\"';}
std::vector<std::string> Application::Tokenize(const std::string& line){std::vector<std::string> t;std::string cur;bool quoted=false,escaping=false;for(char c:line){if(escaping){cur+=c;escaping=false;}else if(c=='\\')escaping=true;else if(c=='\"')quoted=!quoted;else if(!quoted&&std::isspace(static_cast<unsigned char>(c))){if(!cur.empty()){t.push_back(cur);cur.clear();}}else cur+=c;}if(!cur.empty())t.push_back(cur);return t;}
std::string Application::JoinTokens(const std::vector<std::string>& t,std::size_t start){std::string r;for(std::size_t i=start;i<t.size();++i){if(!r.empty())r+=' ';r+=t[i];}return r;}
std::size_t Application::PreviousUtf8Boundary(const std::string& text,std::size_t pos){if(pos==0)return 0;std::size_t r=std::min(pos,text.size())-1;while(r>0&&IsUtf8Continuation(static_cast<unsigned char>(text[r])))--r;return r;}
std::size_t Application::NextUtf8Boundary(const std::string& text,std::size_t pos){if(pos>=text.size())return text.size();std::size_t r=pos+1;while(r<text.size()&&IsUtf8Continuation(static_cast<unsigned char>(text[r])))++r;return r;}

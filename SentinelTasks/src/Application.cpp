#include "Application.hpp"

#include <algorithm>
#include <array>
#include <clocale>
#include <cctype>
#include <limits>
#include <ncurses.h>
#include <regex>
#include <sstream>
#include <string_view>

namespace {

struct CommandDefinition {
    std::string_view name;
    std::string_view description;
};

constexpr std::array<CommandDefinition, 15> Commands{{
    {"addFolder", "add a folder node"},
    {"addTask", "add an individual task"},
    {"remove", "remove a node and its subtree"},
    {"setDescription", "set a node description"},
    {"start", "start/resume a task timer"},
    {"stop", "stop a task timer"},
    {"done", "complete a task and stop timer"},
    {"showTimes", "show timers for all task IDs"},
    {"defineColor", "define a named RGB color"},
    {"color", "color default rows or one node"},
    {"manualSelect", "select nodes with arrows/mouse"},
    {"select", "select a node by ID"},
    {"commands", "show all commands"},
    {"list", "return to the tree view"},
    {"quit", "exit SentinelTasks"}
}};

constexpr std::size_t MaxSuggestions = 6;
constexpr short DefaultTreePair = 1;
constexpr short FirstNodePair = 2;

const std::vector<std::string> CommandHelp{
    "addFolder <id> <parent|root> <name>            Add a folder/category node",
    "addTask <id> <parent|root> <name>              Add an individual task node",
    "remove <id>                                     Remove a node and descendants",
    "setDescription <id> <description>               Set text shown in the right pane",
    "start <task-id>                                  Start/resume a task timer",
    "stop <task-id>                                   Stop a running task timer",
    "done <task-id>                                   Complete a task and stop its timer",
    "showTimes                                        Show all task IDs and timers",
    "defineColor <name> rgb(r,g,b)                   Define/update a named color",
    "color <fg> bg <bg>                              Set default tree-row colors",
    "color <node-id> <fg> bg <bg>                    Set colors for one folder/task",
    "manualSelect [id]                               Enter arrow/mouse selection mode",
    "select <id>                                     Select a node by ID",
    "commands                                        Show this command list",
    "list                                            Return to tree view",
    "quit                                            Exit SentinelTasks",
    "",
    "Colors accept built-in names, defineColor names, or rgb(r,g,b).",
    "Enter an argument-taking command by itself to open its argument window."
};

const std::unordered_map<std::string, RgbColor> BuiltInColors{
    {"black", {0, 0, 0}}, {"red", {205, 49, 49}}, {"green", {13, 188, 121}},
    {"yellow", {229, 229, 16}}, {"blue", {36, 114, 200}}, {"magenta", {188, 63, 188}},
    {"cyan", {17, 168, 205}}, {"white", {229, 229, 229}}, {"brightblack", {102, 102, 102}},
    {"gray", {102, 102, 102}}, {"grey", {102, 102, 102}}, {"brightred", {241, 76, 76}},
    {"brightgreen", {35, 209, 139}}, {"brightyellow", {245, 245, 67}}, {"brightblue", {59, 142, 234}},
    {"brightmagenta", {214, 112, 214}}, {"brightcyan", {41, 184, 219}}, {"brightwhite", {255, 255, 255}}
};

struct DialogRect { int top{}, left{}, height{}, width{}; };

bool IsUtf8Continuation(unsigned char value) { return (value & 0xC0U) == 0x80U; }

std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t");
    return value.substr(first, last - first + 1);
}

std::string NormalizeColorName(std::string value) {
    std::string result;
    for (const unsigned char c : value) if (std::isalnum(c)) result.push_back(static_cast<char>(std::tolower(c)));
    return result;
}

std::optional<RgbColor> ParseRgbExpression(const std::string& value) {
    static const std::regex pattern(R"re(^\s*(?:rgb|rpg)\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*\)\s*$)re", std::regex::icase);
    std::smatch match;
    if (!std::regex_match(value, match, pattern)) return std::nullopt;
    const int r = std::stoi(match[1].str());
    const int g = std::stoi(match[2].str());
    const int b = std::stoi(match[3].str());
    if (r > 255 || g > 255 || b > 255) return std::nullopt;
    return RgbColor{r, g, b};
}

void DrawClipped(int row, int column, int width, const std::string& value) {
    if (width > 0 && column >= 0) mvaddnstr(row, column, value.c_str(), width);
}

std::vector<std::string> WrapText(const std::string& text, int width) {
    std::vector<std::string> lines;
    if (width <= 0) return lines;
    std::istringstream input(text);
    std::string word, line;
    while (input >> word) {
        if (line.empty()) line = word;
        else if (static_cast<int>(line.size() + word.size() + 1) <= width) line += " " + word;
        else { lines.push_back(line); line = word; }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

DialogRect CalculateDialogRect(std::size_t fieldCount) {
    int h = 0, w = 0; getmaxyx(stdscr, h, w);
    DialogRect r;
    r.width = std::clamp(w * 3 / 5, 54, std::max(54, w - 4));
    r.height = std::clamp(9 + static_cast<int>(fieldCount) * 2, 11, std::max(11, h - 4));
    r.left = std::max(0, (w - r.width) / 2);
    r.top = std::max(0, (h - r.height) / 2);
    return r;
}

int DialogFieldRow(const DialogRect& r, std::size_t i) { return r.top + 4 + static_cast<int>(i) * 2; }

short NearestBasicColor(const RgbColor& color) {
    struct Basic { short index; int r, g, b; };
    constexpr std::array<Basic, 8> colors{{
        {COLOR_BLACK,0,0,0},{COLOR_RED,255,0,0},{COLOR_GREEN,0,255,0},{COLOR_YELLOW,255,255,0},
        {COLOR_BLUE,0,0,255},{COLOR_MAGENTA,255,0,255},{COLOR_CYAN,0,255,255},{COLOR_WHITE,255,255,255}
    }};
    long bestDistance = std::numeric_limits<long>::max();
    short best = COLOR_WHITE;
    for (const auto& c : colors) {
        const long dr = color.red-c.r, dg = color.green-c.g, db = color.blue-c.b;
        const long d = dr*dr + dg*dg + db*db;
        if (d < bestDistance) { bestDistance = d; best = c.index; }
    }
    return best;
}

short NearestTerminalColor(const RgbColor& color) {
    if (!has_colors() || COLORS <= 0) return NearestBasicColor(color);
    const int limit = std::min(COLORS, static_cast<int>(std::numeric_limits<short>::max()) + 1);
    long bestDistance = std::numeric_limits<long>::max();
    short best = NearestBasicColor(color);
    bool found = false;
    for (int i = 0; i < limit; ++i) {
        short r=0,g=0,b=0;
        if (color_content(static_cast<short>(i), &r, &g, &b) == ERR) continue;
        const int rr=r*255/1000, gg=g*255/1000, bb=b*255/1000;
        const long dr=color.red-rr, dg=color.green-gg, db=color.blue-bb;
        const long d=dr*dr+dg*dg+db*db;
        if (!found || d<bestDistance) { found=true; bestDistance=d; best=static_cast<short>(i); }
    }
    return best;
}

bool InitializeColorPair(short pair, const RgbColor& foreground, const RgbColor& background) {
    if (!has_colors() || pair <= 0 || pair >= COLOR_PAIRS) return false;
    return init_pair(pair, NearestTerminalColor(foreground), NearestTerminalColor(background)) != ERR;
}

} // namespace

int Application::Run() {
    std::setlocale(LC_ALL, "");
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS, nullptr); timeout(100); curs_set(1);
    if (has_colors()) { start_color(); InitializeColorPair(DefaultTreePair, treeDisplaySettings_.foreground, treeDisplaySettings_.background); }
    while (running_) { HandleInput(); Render(); }
    endwin(); return 0;
}

void Application::HandleInput() {
    const int key = getch(); if (key == ERR) return;
    if (commandDialog_) {
        if (key == KEY_MOUSE) { MEVENT e{}; if (getmouse(&e) == OK) HandleCommandDialogMouse(e.x,e.y); }
        else HandleCommandDialogInput(key);
        return;
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
        else RecallPreviousCommand();
        return;
    }
    if (key == KEY_DOWN) {
        if (navigatingSuggestions_ && !suggestions.empty()) selectedSuggestion_=(selectedSuggestion_+1)%suggestions.size();
        else if (historyIndex_) RecallNextCommand();
        else if (!suggestions.empty()) { navigatingSuggestions_=true; selectedSuggestion_=0; }
        return;
    }
    if (key == '\n' || key == KEY_ENTER) {
        if (navigatingSuggestions_ && !suggestions.empty()) { AcceptSuggestion(suggestions); return; }
        const std::string command = Trim(commandBuffer_);
        if (!command.empty()) { AddCommandToHistory(command); ExecuteCommand(command); }
        commandBuffer_.clear(); cursorPosition_=0; ResetSuggestionNavigation(); return;
    }
    if (key == KEY_BACKSPACE || key == 127 || key == 8) {
        if (cursorPosition_ > 0) { const auto p=PreviousUtf8Boundary(commandBuffer_,cursorPosition_); commandBuffer_.erase(p,cursorPosition_-p); cursorPosition_=p; }
        ResetHistoryNavigation(); ResetSuggestionNavigation(); return;
    }
    if (key >= 32 && key <= 255) {
        commandBuffer_.insert(commandBuffer_.begin()+static_cast<std::ptrdiff_t>(cursorPosition_), static_cast<char>(key));
        ++cursorPosition_; ResetHistoryNavigation(); ResetSuggestionNavigation();
    }
}

void Application::HandleMouse() {
    MEVENT e{}; if (getmouse(&e) != OK) return;
    if ((e.bstate & BUTTON1_CLICKED)==0 && (e.bstate & BUTTON1_PRESSED)==0) return;
    int h=0,w=0; getmaxyx(stdscr,h,w); (void)w;
    if (manualSelect_) { const int i=e.y-2; if (i>=0 && i<static_cast<int>(visibleRowIds_.size())) selectedId_=visibleRowIds_[static_cast<std::size_t>(i)]; return; }
    if (e.y==h-1) { cursorPosition_=std::min(commandBuffer_.size(),static_cast<std::size_t>(std::max(0,e.x-2))); return; }
    const auto s=BuildSuggestions(); if (s.empty()) return;
    const int start=std::max(2,h-2-static_cast<int>(s.size()));
    if (e.y>=start && e.y<start+static_cast<int>(s.size())) { selectedSuggestion_=static_cast<std::size_t>(e.y-start); AcceptSuggestion(s); }
}

void Application::ExecuteCommand(const std::string& line) {
    const auto t=Tokenize(line); if (t.empty()) return; const std::string& c=t[0]; infoLines_.clear();
    if (t.size()==1 && OpenCommandDialog(c)) return;
    if (c=="quit" || c=="exit") { running_=false; return; }
    if (c=="commands") { infoLines_=CommandHelp; status_="Available SentinelTasks commands."; return; }
    if (c=="list") { status_="Tree view."; return; }
    if (c=="showTimes") { ShowTimes(); return; }
    if (c=="defineColor") {
        if (t.size()!=3) status_="Usage: defineColor <name> rgb(r,g,b)";
        else DefineColor(t[1],t[2]);
        return;
    }
    if (c=="color") { ApplyColorCommand(t); return; }
    if (c=="start" || c=="stop" || c=="done") {
        if (t.size()!=2) { status_="Usage: "+c+" <task-id>"; return; }
        std::string error; bool ok=false;
        if (c=="start") ok=tree_.StartTask(t[1],error);
        else if (c=="stop") ok=tree_.StopTask(t[1],error);
        else ok=tree_.CompleteTask(t[1],error);
        status_=ok ? (c=="start"?"Task started: ":c=="stop"?"Task stopped: ":"Task completed: ")+t[1] : error;
        return;
    }
    if (c=="manualSelect") { if(t.size()>2)status_="Usage: manualSelect [id]";else EnterManualSelect(t.size()==2?std::optional<std::string>{t[1]}:std::nullopt);return; }
    if (c=="select") { if(t.size()!=2||!tree_.GetNode(t[1]))status_="Usage: select <existing-id>";else{selectedId_=t[1];status_="Selected: "+selectedId_;}return; }
    if (c=="remove") {
        if(t.size()!=2){status_="Usage: remove <id>";return;}std::string error;
        if(!tree_.RemoveNode(t[1],error))status_=error;else{if(!tree_.GetNode(selectedId_))selectedId_.clear();EnsureSelection();status_="Removed: "+t[1];}return;
    }
    if (c=="setDescription") {
        if(t.size()<3){status_="Usage: setDescription <id> <description>";return;}std::string error;
        status_=tree_.SetDescription(t[1],JoinTokens(t,2),error)?"Description updated: "+t[1]:error;return;
    }
    if (c=="addFolder" || c=="addTask") {
        if(t.size()<4){status_="Usage: "+c+" <id> <parent|root> <name>";return;}std::string error;
        const NodeKind kind=c=="addFolder"?NodeKind::Folder:NodeKind::Task;
        if(!tree_.AddNode(kind,t[1],t[2],JoinTokens(t,3),error))status_=error;
        else{selectedId_=t[1];status_=std::string(kind==NodeKind::Folder?"Folder added: ":"Task added: ")+t[1];}return;
    }
    status_="Unknown command: "+c+". Type 'commands'.";
}

std::optional<RgbColor> Application::ResolveColor(const std::string& value) const {
    const auto custom=definedColors_.find(value); if(custom!=definedColors_.end())return custom->second;
    const auto builtIn=BuiltInColors.find(NormalizeColorName(value)); if(builtIn!=BuiltInColors.end())return builtIn->second;
    return ParseRgbExpression(value);
}

bool Application::DefineColor(const std::string& id,const std::string& rgb) {
    if(id.empty()){status_="Color name cannot be empty.";return false;}const auto c=ParseRgbExpression(rgb);
    if(!c){status_="Usage: defineColor <name> rgb(r,g,b), channels 0-255.";return false;}definedColors_[id]=*c;status_="Color defined: "+id;return true;
}

bool Application::ApplyColorCommand(const std::vector<std::string>& t) {
    if(t.size()==4&&t[2]=="bg"){
        const auto fg=ResolveColor(t[1]),bg=ResolveColor(t[3]);if(!fg||!bg){status_="Unknown color.";return false;}
        treeDisplaySettings_.foreground=*fg;treeDisplaySettings_.background=*bg;if(has_colors())InitializeColorPair(DefaultTreePair,*fg,*bg);status_="Default tree-row colors updated.";return true;
    }
    if(t.size()==5&&t[3]=="bg"){
        const auto fg=ResolveColor(t[2]),bg=ResolveColor(t[4]);if(!fg||!bg){status_="Unknown color.";return false;}std::string error;
        if(!tree_.SetColor(t[1],*fg,*bg,error)){status_=error;return false;}status_="Node colors updated: "+t[1];return true;
    }
    status_="Usage: color <fg> bg <bg> OR color <node-id> <fg> bg <bg>";return false;
}

void Application::ShowTimes() {
    infoLines_.clear();
    infoLines_.push_back("Task timers:");
    bool any=false;
    for(const auto& entry:tree_.Flatten()){
        if(!entry.node||entry.node->kind!=NodeKind::Task)continue;
        any=true;
        const auto& n=*entry.node;
        const std::string state=n.completed?"done":n.running?"running":"idle";
        infoLines_.push_back(n.id+"  "+n.ElapsedString()+"  ["+state+"]  completed: "+n.CompletionString());
    }
    if(!any)infoLines_.push_back("(No task nodes)");
    status_="Showing timers for all tasks.";
}

void Application::Render() {
    erase();attr_set(A_NORMAL,0,nullptr);EnsureSelection();RenderHeader();RenderTree();RenderDescriptionPane();
    if(!manualSelect_&&!commandDialog_)RenderSuggestions(BuildSuggestions());RenderStatus();RenderCommandLine();if(commandDialog_)RenderCommandDialog();refresh();
}

void Application::RenderHeader() {
    attr_set(A_NORMAL,0,nullptr);int h=0,w=0;getmaxyx(stdscr,h,w);(void)h;
    const std::string title=commandDialog_?"SentinelTasks | COMMAND ARGUMENT WINDOW":manualSelect_?"SentinelTasks | MANUAL SELECT":"SentinelTasks | Tree Task Planner";
    DrawClipped(0,0,std::max(0,w-1),title);if(w>1)mvhline(1,0,ACS_HLINE,w-1);
}

void Application::RenderTree() {
    int h=0,w=0;getmaxyx(stdscr,h,w);const int divider=std::clamp(w*64/100,40,std::max(40,w-30));const int treeWidth=std::max(1,divider-2);const int last=std::max(2,h-3);
    visibleRowIds_.clear();
    if(!infoLines_.empty()){attr_set(A_NORMAL,0,nullptr);int row=2;for(const auto& l:infoLines_){if(row>=last)break;DrawClipped(row++,0,treeWidth,l);}return;}
    const auto visible=tree_.Flatten();int row=2;std::size_t visibleIndex=0;
    for(const auto& e:visible){if(row>=last||!e.node)break;const auto& n=*e.node;const bool selected=n.id==selectedId_;short pair=DefaultTreePair;
        if(has_colors()&&n.foregroundColor&&n.backgroundColor){const short candidate=static_cast<short>(FirstNodePair+visibleIndex);if(candidate>0&&candidate<COLOR_PAIRS&&InitializeColorPair(candidate,*n.foregroundColor,*n.backgroundColor))pair=candidate;}
        if(has_colors())attr_set(selected?A_REVERSE:A_NORMAL,pair,nullptr);else if(selected)attron(A_REVERSE);
        const std::string kind=n.kind==NodeKind::Folder?"[F] ":n.completed?"[x] ":n.running?"[>] ":"[T] ";
        DrawClipped(row,0,treeWidth,e.connectorPrefix+kind+n.name+"  {"+n.id+"}");attr_set(A_NORMAL,0,nullptr);visibleRowIds_.push_back(n.id);++row;++visibleIndex;}
    if(visible.empty())DrawClipped(3,0,treeWidth,"No nodes yet. Use addFolder or addTask.");
}

void Application::RenderDescriptionPane() {
    attr_set(A_NORMAL,0,nullptr);int h=0,w=0;getmaxyx(stdscr,h,w);const int d=std::clamp(w*64/100,40,std::max(40,w-30));if(d>=w-2)return;
    for(int r=2;r<h-2;++r)mvaddch(r,d,ACS_VLINE);const int col=d+2,pw=std::max(1,w-col-1);DrawClipped(2,col,pw,"Description / Timing");mvhline(3,col,ACS_HLINE,pw);
    const TaskNode* n=tree_.GetNode(selectedId_);if(!n){DrawClipped(5,col,pw,"No task selected.");return;}
    DrawClipped(5,col,pw,"ID: "+n->id);DrawClipped(6,col,pw,"Type: "+std::string(n->kind==NodeKind::Folder?"folder":"task"));
    int descriptionStart=9;
    if(n->kind==NodeKind::Task){
        const std::string state=n->completed?"completed":n->running?"running":"idle";
        DrawClipped(7,col,pw,"Timer: "+n->ElapsedString()+"   State: "+state);
        DrawClipped(8,col,pw,"Completed: "+n->CompletionString());descriptionStart=11;
    }
    DrawClipped(descriptionStart-1,col,pw,n->name);
    const auto lines=WrapText(n->description.empty()?"(No description)":n->description,pw);int row=descriptionStart+1;
    for(const auto& l:lines){if(row>=h-3)break;DrawClipped(row++,col,pw,l);}
}

void Application::RenderSuggestions(const std::vector<Suggestion>& s) {
    if(s.empty())return;attr_set(A_NORMAL,0,nullptr);int h=0,w=0;getmaxyx(stdscr,h,w);const int start=std::max(2,h-2-static_cast<int>(s.size()));
    for(std::size_t i=0;i<s.size();++i){if(i==selectedSuggestion_)attron(A_REVERSE);DrawClipped(start+static_cast<int>(i),0,w-1,(i==selectedSuggestion_?"> ":"  ")+s[i].label);if(i==selectedSuggestion_)attroff(A_REVERSE);}
}

void Application::RenderStatus(){attr_set(A_NORMAL,0,nullptr);int h=0,w=0;getmaxyx(stdscr,h,w);std::string s=status_;if(commandDialog_)s+=" | Tab fields | Enter choose | F2 submit | Esc cancel";else if(manualSelect_)s+=" | Up/Down select | Left parent | Right child | Enter/Esc finish";DrawClipped(h-2,0,w-1,s);}

void Application::RenderCommandLine(){attr_set(A_NORMAL,0,nullptr);int h=0,w=0;getmaxyx(stdscr,h,w);move(h-1,0);clrtoeol();if(commandDialog_){curs_set(0);DrawClipped(h-1,0,w-1,"> [argument window: "+commandDialog_->command+"]");return;}if(manualSelect_){curs_set(0);DrawClipped(h-1,0,w-1,"> [manualSelect] "+selectedId_);return;}curs_set(1);DrawClipped(h-1,0,2,"> ");DrawClipped(h-1,2,w-3,commandBuffer_);move(h-1,std::min(w-1,static_cast<int>(cursorPosition_)+2));}

void Application::RenderCommandDialog(){if(!commandDialog_)return;attr_set(A_NORMAL,0,nullptr);auto& d=*commandDialog_;const auto r=CalculateDialogRect(d.fields.size());
    for(int y=r.top;y<r.top+r.height;++y){move(y,r.left);for(int x=0;x<r.width;++x)addch(' ');}mvhline(r.top,r.left+1,ACS_HLINE,r.width-2);mvhline(r.top+r.height-1,r.left+1,ACS_HLINE,r.width-2);mvvline(r.top+1,r.left,ACS_VLINE,r.height-2);mvvline(r.top+1,r.left+r.width-1,ACS_VLINE,r.height-2);mvaddch(r.top,r.left,ACS_ULCORNER);mvaddch(r.top,r.left+r.width-1,ACS_URCORNER);mvaddch(r.top+r.height-1,r.left,ACS_LLCORNER);mvaddch(r.top+r.height-1,r.left+r.width-1,ACS_LRCORNER);
    DrawClipped(r.top+1,r.left+3,r.width-6,d.title);DrawClipped(r.top+2,r.left+3,r.width-6,"Fill arguments manually. CLI arguments remain supported.");const int lw=std::min(18,std::max(10,r.width/4));const int ic=r.left+3+lw,iw=std::max(10,r.width-lw-7);
    for(std::size_t i=0;i<d.fields.size();++i){auto& f=d.fields[i];const int row=DialogFieldRow(r,i);DrawClipped(row,r.left+3,lw-1,f.label+":");if(d.focusedControl==i)attron(A_REVERSE);const std::string v=f.value.empty()&&f.kind==DialogFieldKind::DropList?"(no options)":f.value;DrawClipped(row,ic,iw,"[ "+v+(f.kind==DialogFieldKind::DropList?"  v":"")+" ]");if(d.focusedControl==i)attroff(A_REVERSE);}
    const std::size_t submit=d.fields.size(),cancel=submit+1;const int br=r.top+r.height-3,sc=r.left+r.width/2-14,cc=r.left+r.width/2+3;if(d.focusedControl==submit)attron(A_REVERSE);DrawClipped(br,sc,12,"[ Submit ]");if(d.focusedControl==submit)attroff(A_REVERSE);if(d.focusedControl==cancel)attron(A_REVERSE);DrawClipped(br,cc,12,"[ Cancel ]");if(d.focusedControl==cancel)attroff(A_REVERSE);DrawClipped(r.top+r.height-2,r.left+3,r.width-6,d.validationMessage);
    if(d.focusedControl<d.fields.size()&&d.fields[d.focusedControl].kind==DialogFieldKind::TextInput){curs_set(1);move(DialogFieldRow(r,d.focusedControl),std::min(ic+iw-2,ic+2+static_cast<int>(d.fields[d.focusedControl].cursor)));}else curs_set(0);
}

std::vector<Application::Suggestion> Application::BuildSuggestions() const{std::vector<Suggestion> result;if(commandBuffer_.empty()||commandBuffer_.find_first_of(" \t")!=std::string::npos)return result;std::string q=commandBuffer_;std::transform(q.begin(),q.end(),q.begin(),[](unsigned char v){return static_cast<char>(std::tolower(v));});for(const auto& c:Commands){std::string name(c.name),lower=name;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char v){return static_cast<char>(std::tolower(v));});if(lower.find(q)!=std::string::npos)result.push_back({name+"  -  "+std::string(c.description),name+" "});if(result.size()==MaxSuggestions)break;}return result;}
void Application::AcceptSuggestion(const std::vector<Suggestion>& s){if(s.empty())return;selectedSuggestion_=std::min(selectedSuggestion_,s.size()-1);commandBuffer_=s[selectedSuggestion_].replacement;cursorPosition_=commandBuffer_.size();ResetSuggestionNavigation();}
void Application::ResetSuggestionNavigation(){selectedSuggestion_=0;navigatingSuggestions_=false;}
void Application::AddCommandToHistory(const std::string& c){if(!c.empty()&&(commandHistory_.empty()||commandHistory_.back()!=c))commandHistory_.push_back(c);ResetHistoryNavigation();}
void Application::RecallPreviousCommand(){if(commandHistory_.empty())return;if(!historyIndex_){commandBeforeHistory_=commandBuffer_;historyIndex_=commandHistory_.size()-1;}else if(*historyIndex_>0)--*historyIndex_;commandBuffer_=commandHistory_[*historyIndex_];cursorPosition_=commandBuffer_.size();}
void Application::RecallNextCommand(){if(!historyIndex_)return;if(*historyIndex_+1<commandHistory_.size()){++*historyIndex_;commandBuffer_=commandHistory_[*historyIndex_];}else{commandBuffer_=commandBeforeHistory_;ResetHistoryNavigation();}cursorPosition_=commandBuffer_.size();}
void Application::ResetHistoryNavigation(){historyIndex_.reset();commandBeforeHistory_.clear();}
void Application::EnterManualSelect(const std::optional<std::string>& id){if(tree_.Empty()){status_="Cannot enter manualSelect: tree is empty.";return;}if(id){if(!tree_.GetNode(*id)){status_="Node ID does not exist: "+*id;return;}selectedId_=*id;}EnsureSelection();manualSelect_=true;status_="Manual selection mode.";}
void Application::LeaveManualSelect(){manualSelect_=false;curs_set(1);}
void Application::MoveManualSelection(int delta){const auto v=tree_.Flatten();if(v.empty())return;std::size_t i=0;for(std::size_t j=0;j<v.size();++j)if(v[j].node&&v[j].node->id==selectedId_){i=j;break;}i=delta<0?(i==0?v.size()-1:i-1):(i+1)%v.size();selectedId_=v[i].node->id;}
void Application::SelectParent(){if(const auto p=tree_.ParentOf(selectedId_))selectedId_=*p;}
void Application::SelectFirstChild(){if(const auto c=tree_.FirstChildOf(selectedId_))selectedId_=*c;}
void Application::EnsureSelection(){if(!selectedId_.empty()&&tree_.GetNode(selectedId_))return;const auto v=tree_.Flatten();selectedId_=v.empty()?std::string{}:v.front().node->id;}

bool Application::OpenCommandDialog(const std::string& command){CommandDialog d;d.command=command;d.title="Command: "+command;auto text=[](std::string label,std::string value={}){DialogField f;f.label=std::move(label);f.value=std::move(value);f.cursor=f.value.size();return f;};auto drop=[&](std::string label,std::vector<std::string> options,std::string preferred={}){DialogField f;f.label=std::move(label);f.kind=DialogFieldKind::DropList;f.options=std::move(options);f.selectedOption=FindOptionIndex(f.options,preferred).value_or(0);if(!f.options.empty())f.value=f.options[f.selectedOption];return f;};
    if(command=="addFolder"||command=="addTask"){d.fields.push_back(text("ID"));std::string parent="root";if(const auto* n=tree_.GetNode(selectedId_)){if(n->kind==NodeKind::Folder)parent=n->id;else if(!n->parentId.empty())parent=n->parentId;}d.fields.push_back(drop("Parent",NodeIdOptions(true,true),parent));d.fields.push_back(text("Name"));}
    else if(command=="setDescription"){d.fields.push_back(drop("Node",NodeIdOptions(false,false),selectedId_));std::string description;if(const auto* n=tree_.GetNode(selectedId_))description=n->description;d.fields.push_back(text("Description",description));}
    else if(command=="remove"||command=="select"||command=="manualSelect")d.fields.push_back(drop("Node",NodeIdOptions(false,false),selectedId_));
    else if(command=="start"||command=="stop"||command=="done")d.fields.push_back(drop("Task",TaskIdOptions(),selectedId_));
    else if(command=="defineColor"){d.fields.push_back(text("Color name"));d.fields.push_back(text("RGB","rgb(255,255,255)"));}
    else if(command=="color"){auto targets=NodeIdOptions(false,false);targets.insert(targets.begin(),"default");d.fields.push_back(drop("Target",std::move(targets),selectedId_.empty()?"default":selectedId_));d.fields.push_back(drop("Foreground",ColorNameOptions(),"white"));d.fields.push_back(drop("Background",ColorNameOptions(),"black"));}
    else return false;commandDialog_=std::move(d);status_="Argument window opened for: "+command;return true;}
void Application::CloseCommandDialog(){commandDialog_.reset();curs_set(1);}
void Application::HandleCommandDialogInput(int key){if(!commandDialog_)return;auto& d=*commandDialog_;if(key==27){CloseCommandDialog();return;}if(key==KEY_F(2)){SubmitCommandDialog();return;}if(key=='\t'){MoveDialogFocus(1);return;}const std::size_t submit=d.fields.size(),cancel=submit+1;if(d.focusedControl==submit){if(key=='\n'||key==KEY_ENTER||key==' ')SubmitCommandDialog();return;}if(d.focusedControl==cancel){if(key=='\n'||key==KEY_ENTER||key==' ')CloseCommandDialog();return;}if(d.focusedControl>=d.fields.size())return;auto& f=d.fields[d.focusedControl];if(f.kind==DialogFieldKind::DropList){if(f.options.empty())return;if(key==KEY_UP)f.selectedOption=f.selectedOption==0?f.options.size()-1:f.selectedOption-1;else if(key==KEY_DOWN)f.selectedOption=(f.selectedOption+1)%f.options.size();else if(key=='\n'||key==KEY_ENTER){MoveDialogFocus(1);return;}f.value=f.options[f.selectedOption];return;}if(key==KEY_LEFT)f.cursor=PreviousUtf8Boundary(f.value,f.cursor);else if(key==KEY_RIGHT)f.cursor=NextUtf8Boundary(f.value,f.cursor);else if(key==KEY_BACKSPACE||key==127||key==8){if(f.cursor>0){const auto p=PreviousUtf8Boundary(f.value,f.cursor);f.value.erase(p,f.cursor-p);f.cursor=p;}}else if(key=='\n'||key==KEY_ENTER)MoveDialogFocus(1);else if(key>=32&&key<=255){f.value.insert(f.value.begin()+static_cast<std::ptrdiff_t>(f.cursor),static_cast<char>(key));++f.cursor;}}
void Application::HandleCommandDialogMouse(int x,int y){if(!commandDialog_)return;auto& d=*commandDialog_;const auto r=CalculateDialogRect(d.fields.size());const int lw=std::min(18,std::max(10,r.width/4)),ic=r.left+3+lw,iw=std::max(10,r.width-lw-7);for(std::size_t i=0;i<d.fields.size();++i)if(y==DialogFieldRow(r,i)&&x>=ic&&x<ic+iw){d.focusedControl=i;return;}const int br=r.top+r.height-3,sc=r.left+r.width/2-14,cc=r.left+r.width/2+3;if(y==br&&x>=sc&&x<sc+12)SubmitCommandDialog();else if(y==br&&x>=cc&&x<cc+12)CloseCommandDialog();}
void Application::MoveDialogFocus(int delta){if(!commandDialog_)return;auto& d=*commandDialog_;const std::size_t count=d.fields.size()+2;d.focusedControl=delta<0?(d.focusedControl==0?count-1:d.focusedControl-1):(d.focusedControl+1)%count;}
bool Application::SubmitCommandDialog(){if(!commandDialog_)return false;for(const auto& f:commandDialog_->fields)if(f.value.empty()){commandDialog_->validationMessage="Required field is empty: "+f.label;return false;}const std::string line=BuildDialogCommand();if(line.empty())return false;CloseCommandDialog();AddCommandToHistory(line);ExecuteCommand(line);return true;}
std::string Application::BuildDialogCommand() const{if(!commandDialog_)return{};const auto& d=*commandDialog_;if(d.command=="color"&&d.fields.size()==3){const auto& target=d.fields[0].value;const auto& fg=d.fields[1].value;const auto& bg=d.fields[2].value;if(target=="default")return "color "+QuoteArgument(fg)+" bg "+QuoteArgument(bg);return "color "+QuoteArgument(target)+" "+QuoteArgument(fg)+" bg "+QuoteArgument(bg);}std::string r=d.command;for(const auto& f:d.fields)r+=" "+QuoteArgument(f.value);return r;}
std::vector<std::string> Application::NodeIdOptions(bool foldersOnly,bool includeRoot) const{std::vector<std::string> o;if(includeRoot)o.push_back("root");for(const auto& v:tree_.Flatten())if(v.node&&(!foldersOnly||v.node->kind==NodeKind::Folder))o.push_back(v.node->id);return o;}
std::vector<std::string> Application::TaskIdOptions() const{std::vector<std::string> o;for(const auto& v:tree_.Flatten())if(v.node&&v.node->kind==NodeKind::Task)o.push_back(v.node->id);return o;}
std::vector<std::string> Application::ColorNameOptions() const{std::vector<std::string> o{"black","red","green","yellow","blue","magenta","cyan","white","brightBlack","brightRed","brightGreen","brightYellow","brightBlue","brightMagenta","brightCyan","brightWhite"};for(const auto& [name,color]:definedColors_){(void)color;o.push_back(name);}std::sort(o.begin(),o.end());o.erase(std::unique(o.begin(),o.end()),o.end());return o;}
std::optional<std::size_t> Application::FindOptionIndex(const std::vector<std::string>& o,const std::string& v) const{const auto it=std::find(o.begin(),o.end(),v);return it==o.end()?std::nullopt:std::optional<std::size_t>{static_cast<std::size_t>(std::distance(o.begin(),it))};}
std::string Application::QuoteArgument(const std::string& v){std::string r="\"";for(const char c:v){if(c=='\\'||c=='\"')r+='\\';r+=c;}r+='\"';return r;}
std::vector<std::string> Application::Tokenize(const std::string& line){std::vector<std::string> t;std::string cur;bool quoted=false,escaping=false;for(const char c:line){if(escaping){cur+=c;escaping=false;}else if(c=='\\')escaping=true;else if(c=='\"')quoted=!quoted;else if(!quoted&&std::isspace(static_cast<unsigned char>(c))){if(!cur.empty()){t.push_back(cur);cur.clear();}}else cur+=c;}if(!cur.empty())t.push_back(cur);return t;}
std::string Application::JoinTokens(const std::vector<std::string>& t,std::size_t start){std::string r;for(std::size_t i=start;i<t.size();++i){if(!r.empty())r+=' ';r+=t[i];}return r;}
std::size_t Application::PreviousUtf8Boundary(const std::string& text,std::size_t pos){if(pos==0)return 0;std::size_t r=std::min(pos,text.size())-1;while(r>0&&IsUtf8Continuation(static_cast<unsigned char>(text[r])))--r;return r;}
std::size_t Application::NextUtf8Boundary(const std::string& text,std::size_t pos){if(pos>=text.size())return text.size();std::size_t r=pos+1;while(r<text.size()&&IsUtf8Continuation(static_cast<unsigned char>(text[r])))++r;return r;}

#include "Application.hpp"
#include "FuzzySearch.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <ncurses.h>
#include <sstream>
#include <string_view>

namespace {

constexpr std::size_t MaxSuggestions = 6;
constexpr short TaskColorPair = 1;
constexpr short FirstPerTaskPair = 2;

struct CommandDefinition { std::string_view name; std::string_view description; };
struct DialogRect { int top{}, left{}, height{}, width{}; };

constexpr std::array<CommandDefinition, 13> Commands{{
    {"add", "add a new task with an ID"}, {"remove", "remove a task"},
    {"start", "start or resume a task"}, {"stop", "stop a running task"},
    {"done", "complete a task"}, {"search", "fuzzy-search tasks"},
    {"list", "show all tasks"}, {"commands", "show all commands"},
    {"setJsonFile", "switch active JSON data file"}, {"defineColor", "define a named RGB color"},
    {"color", "set default or per-task color"}, {"help", "show command help"},
    {"quit", "save and exit Sentinel"},
}};

bool TakesTaskArgument(std::string_view command) {
    return command == "remove" || command == "start" || command == "stop" || command == "done" || command == "search";
}

std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t") - first + 1);
}

std::string TrimLeft(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    return first == std::string::npos ? std::string{} : value.substr(first);
}

std::string UnquotePartial(std::string value) {
    value = TrimLeft(std::move(value));
    if (!value.empty() && value.front() == '"') value.erase(value.begin());
    if (!value.empty() && value.back() == '"') value.pop_back();
    return value;
}

std::string QuoteIfNeeded(const std::string& value) {
    if (value.find_first_of(" \t\"") == std::string::npos) return value;
    std::string result = "\"";
    for (char c : value) { if (c == '\\' || c == '"') result += '\\'; result += c; }
    result += '"';
    return result;
}

DialogRect CalculateDialogRect(std::size_t fields) {
    int h=0,w=0; getmaxyx(stdscr,h,w);
    DialogRect r;
    r.width = std::clamp(w * 3 / 5, 54, std::max(54, w - 4));
    r.height = std::clamp(9 + static_cast<int>(fields) * 2, 11, std::max(11, h - 4));
    r.left = std::max(0, (w-r.width)/2); r.top = std::max(0, (h-r.height)/2);
    return r;
}

int DialogFieldRow(const DialogRect& r, std::size_t index) { return r.top + 4 + static_cast<int>(index) * 2; }

short NearestBasicColor(const RgbColor& color) {
    struct BasicColor { short ncursesColor; int red; int green; int blue; };
    constexpr std::array<BasicColor, 8> basic{{
        {COLOR_BLACK,0,0,0},{COLOR_RED,255,0,0},{COLOR_GREEN,0,255,0},{COLOR_YELLOW,255,255,0},
        {COLOR_BLUE,0,0,255},{COLOR_MAGENTA,255,0,255},{COLOR_CYAN,0,255,255},{COLOR_WHITE,255,255,255}
    }};
    long bestDistance=std::numeric_limits<long>::max(); short best=COLOR_WHITE;
    for (const auto& candidate : basic) {
        const long dr=color.red-candidate.red,dg=color.green-candidate.green,db=color.blue-candidate.blue;
        const long distance=dr*dr+dg*dg+db*db;
        if (distance<bestDistance) { bestDistance=distance; best=candidate.ncursesColor; }
    }
    return best;
}

short NearestTerminalColor(const RgbColor& color) {
    if (!has_colors() || COLORS <= 0) return NearestBasicColor(color);
    const int limit=std::min(COLORS,static_cast<int>(std::numeric_limits<short>::max())+1);
    long bestDistance=std::numeric_limits<long>::max(); short best=NearestBasicColor(color); bool found=false;
    for (int i=0;i<limit;++i) {
        short r=0,g=0,b=0; if (color_content(static_cast<short>(i),&r,&g,&b)==ERR) continue;
        const long dr=color.red-r*255/1000,dg=color.green-g*255/1000,db=color.blue-b*255/1000;
        const long distance=dr*dr+dg*dg+db*db;
        if (!found || distance<bestDistance) { found=true; bestDistance=distance; best=static_cast<short>(i); }
    }
    return best;
}

bool InitializeTaskColorPair(short pair,const RgbColor& fg,const RgbColor& bg) {
    if (!has_colors() || pair<=0 || pair>=COLOR_PAIRS) return false;
    return init_pair(pair,NearestTerminalColor(fg),NearestTerminalColor(bg)) != ERR;
}

void DrawClippedField(int row,int column,int maxWidth,const std::string& text) {
    if (maxWidth>0 && column>=0) mvaddnstr(row,column,text.c_str(),maxWidth);
}

} // namespace

Application::Application()
    : commandProcessor_(taskManager_,displaySettings_), lastAutosave_(std::chrono::steady_clock::now()) {
    std::string error; if (!taskManager_.Load(error)) persistenceStatus_="Load failed: "+error;
}

int Application::Run() {
    initscr(); cbreak(); noecho(); keypad(stdscr,TRUE); curs_set(1); timeout(100); mousemask(ALL_MOUSE_EVENTS,nullptr);
    if (has_colors()) start_color(); ApplyColors();
    while (!commandProcessor_.ShouldQuit()) { HandleInput(); PeriodicAutosave(); if(displaySettings_.dirty)ApplyColors(); Render(); }
    std::string error; taskManager_.Save(error); endwin(); return 0;
}

void Application::PeriodicAutosave() {
    const auto now=std::chrono::steady_clock::now(); if(now-lastAutosave_<std::chrono::seconds(1))return;
    lastAutosave_=now; std::string error; if(!taskManager_.Save(error))persistenceStatus_="Autosave failed: "+error;
}

void Application::ApplyColors() {
    displaySettings_.dirty=false;
    if(!has_colors()){persistenceStatus_="Terminal does not support colors.";return;}
    if(!InitializeTaskColorPair(TaskColorPair,displaySettings_.foreground,displaySettings_.background))persistenceStatus_="Terminal could not initialize the default task color pair.";
    bkgd(A_NORMAL); attr_set(A_NORMAL,0,nullptr);
}

void Application::AddCommandToHistory(const std::string& command) {
    if(command.empty())return; if(commandHistory_.empty()||commandHistory_.back()!=command)commandHistory_.push_back(command); ResetHistoryNavigation();
}
void Application::RecallPreviousCommand(){if(commandHistory_.empty())return;if(!historyIndex_){commandBeforeHistory_=commandBuffer_;historyIndex_=commandHistory_.size()-1;}else if(*historyIndex_>0)--*historyIndex_;commandBuffer_=commandHistory_[*historyIndex_];cursorPosition_=commandBuffer_.size();ResetSuggestionSelection();}
void Application::RecallNextCommand(){if(!historyIndex_)return;if(*historyIndex_+1<commandHistory_.size()){++*historyIndex_;commandBuffer_=commandHistory_[*historyIndex_];}else{commandBuffer_=commandBeforeHistory_;ResetHistoryNavigation();}cursorPosition_=commandBuffer_.size();ResetSuggestionSelection();}
void Application::ResetHistoryNavigation(){historyIndex_.reset();commandBeforeHistory_.clear();}

void Application::HandleInput() {
    const int key=getch(); if(key==ERR)return;
    if(commandDialog_){if(key==KEY_MOUSE){MEVENT e{};if(getmouse(&e)==OK)HandleCommandDialogMouse(e.x,e.y);}else HandleCommandDialogInput(key);return;}
    const auto suggestions=BuildSuggestions();
    if(key==KEY_MOUSE){HandleMouse();return;}
    if(key=='\t'){AcceptSuggestion(suggestions);ResetHistoryNavigation();return;}
    if(key==KEY_LEFT){if(cursorPosition_>0)--cursorPosition_;ResetHistoryNavigation();ResetSuggestionSelection();return;}
    if(key==KEY_RIGHT){if(cursorPosition_<commandBuffer_.size())++cursorPosition_;ResetHistoryNavigation();ResetSuggestionSelection();return;}
    if(key==KEY_UP){if(navigatingSuggestions_&&!suggestions.empty())selectedSuggestion_=selectedSuggestion_==0?suggestions.size()-1:selectedSuggestion_-1;else RecallPreviousCommand();return;}
    if(key==KEY_DOWN){if(navigatingSuggestions_&&!suggestions.empty())selectedSuggestion_=(selectedSuggestion_+1)%suggestions.size();else if(historyIndex_)RecallNextCommand();else if(!suggestions.empty()){navigatingSuggestions_=true;selectedSuggestion_=0;}return;}
    if(key=='\n'||key==KEY_ENTER){
        if(navigatingSuggestions_&&!suggestions.empty()){AcceptSuggestion(suggestions);return;}
        const std::string executed=Trim(commandBuffer_);
        const auto separator=executed.find_first_of(" \t");
        const std::string command=executed.substr(0,separator);
        if(!executed.empty()&&separator==std::string::npos&&OpenCommandDialog(command)){commandBuffer_.clear();cursorPosition_=0;ResetSuggestionSelection();return;}
        commandProcessor_.Execute(executed);AddCommandToHistory(executed);commandBuffer_.clear();cursorPosition_=0;persistenceStatus_.clear();ResetSuggestionSelection();return;
    }
    if(key==KEY_BACKSPACE||key==127||key==8){if(cursorPosition_>0&&!commandBuffer_.empty()){commandBuffer_.erase(cursorPosition_-1,1);--cursorPosition_;}ResetHistoryNavigation();ResetSuggestionSelection();return;}
    if(key>=32&&key<=126){commandBuffer_.insert(commandBuffer_.begin()+static_cast<std::ptrdiff_t>(cursorPosition_),static_cast<char>(key));++cursorPosition_;ResetHistoryNavigation();ResetSuggestionSelection();}
}

void Application::HandleMouse() {
    MEVENT event{};if(getmouse(&event)!=OK)return;if((event.bstate&BUTTON1_CLICKED)==0&&(event.bstate&BUTTON1_PRESSED)==0)return;
    int h=0,w=0;getmaxyx(stdscr,h,w);(void)w;
    if(event.y==h-1){cursorPosition_=std::min(commandBuffer_.size(),static_cast<std::size_t>(std::max(0,event.x-2)));ResetHistoryNavigation();ResetSuggestionSelection();return;}
    const auto suggestions=BuildSuggestions();if(suggestions.empty())return;const int start=SuggestionStartRow(suggestions.size());
    if(event.y>=start&&event.y<start+static_cast<int>(suggestions.size())){selectedSuggestion_=static_cast<std::size_t>(event.y-start);navigatingSuggestions_=true;AcceptSuggestion(suggestions);ResetHistoryNavigation();}
}

void Application::AcceptSuggestion(const std::vector<Suggestion>& suggestions){if(suggestions.empty())return;selectedSuggestion_=std::min(selectedSuggestion_,suggestions.size()-1);commandBuffer_=suggestions[selectedSuggestion_].replacement;cursorPosition_=commandBuffer_.size();ResetSuggestionSelection();}
void Application::ResetSuggestionSelection(){selectedSuggestion_=0;navigatingSuggestions_=false;}

bool Application::OpenCommandDialog(const std::string& command) {
    CommandDialog dialog; dialog.command=command; dialog.title="Command: "+command;
    auto text=[](std::string label,std::string value={}){DialogField f;f.label=std::move(label);f.value=std::move(value);f.cursor=f.value.size();return f;};
    auto drop=[](std::string label,std::vector<std::string> options,std::string preferred={}){DialogField f;f.label=std::move(label);f.kind=DialogFieldKind::DropList;f.options=std::move(options);if(!f.options.empty()){auto it=std::find(f.options.begin(),f.options.end(),preferred);f.selectedOption=it==f.options.end()?0:static_cast<std::size_t>(std::distance(f.options.begin(),it));f.value=f.options[f.selectedOption];}return f;};
    if(command=="add"){dialog.fields.push_back(text("ID"));dialog.fields.push_back(text("Name"));}
    else if(command=="remove"||command=="start"||command=="stop"||command=="done"||command=="search"){dialog.fields.push_back(drop(command=="search"?"Task / query":"Task",TaskIdOptions()));}
    else if(command=="setJsonFile")dialog.fields.push_back(text("JSON path",taskManager_.GetJsonFile()));
    else if(command=="defineColor"){dialog.fields.push_back(text("Color name"));dialog.fields.push_back(text("RGB","rgb(255,255,255)"));}
    else if(command=="color"){auto targets=TaskIdOptions();targets.insert(targets.begin(),"default");dialog.fields.push_back(drop("Target",std::move(targets),"default"));dialog.fields.push_back(drop("Foreground",ColorNameOptions(),"white"));dialog.fields.push_back(drop("Background",ColorNameOptions(),"black"));}
    else return false;
    commandDialog_=std::move(dialog);persistenceStatus_="Argument window opened for: "+command;return true;
}

void Application::CloseCommandDialog(){commandDialog_.reset();curs_set(1);}

void Application::HandleCommandDialogInput(int key) {
    if(!commandDialog_)return;auto& d=*commandDialog_;
    if(key==27){CloseCommandDialog();return;}
    if(key==KEY_F(2)){SubmitCommandDialog();return;}
    if(key=='\t'){MoveDialogFocus(1);return;}
#ifdef KEY_BTAB
    if(key==KEY_BTAB){MoveDialogFocus(-1);return;}
#endif
    const std::size_t submit=d.fields.size(),cancel=submit+1;
    if(d.focusedControl==submit){if(key=='\n'||key==KEY_ENTER||key==' ')SubmitCommandDialog();return;}
    if(d.focusedControl==cancel){if(key=='\n'||key==KEY_ENTER||key==' ')CloseCommandDialog();return;}
    if(d.focusedControl>=d.fields.size())return;auto& f=d.fields[d.focusedControl];
    if(f.kind==DialogFieldKind::DropList){if(f.options.empty())return;if(key==KEY_UP)f.selectedOption=f.selectedOption==0?f.options.size()-1:f.selectedOption-1;else if(key==KEY_DOWN)f.selectedOption=(f.selectedOption+1)%f.options.size();else if(key=='\n'||key==KEY_ENTER){MoveDialogFocus(1);return;}f.value=f.options[f.selectedOption];return;}
    if(key==KEY_LEFT){if(f.cursor>0)--f.cursor;}else if(key==KEY_RIGHT){if(f.cursor<f.value.size())++f.cursor;}else if(key==KEY_BACKSPACE||key==127||key==8){if(f.cursor>0){f.value.erase(f.cursor-1,1);--f.cursor;}}else if(key=='\n'||key==KEY_ENTER)MoveDialogFocus(1);else if(key>=32&&key<=126){f.value.insert(f.value.begin()+static_cast<std::ptrdiff_t>(f.cursor),static_cast<char>(key));++f.cursor;}
}

void Application::HandleCommandDialogMouse(int x,int y) {
    if(!commandDialog_)return;auto& d=*commandDialog_;const auto r=CalculateDialogRect(d.fields.size());const int labelWidth=std::min(18,std::max(10,r.width/4));const int inputCol=r.left+3+labelWidth;const int inputWidth=std::max(10,r.width-labelWidth-7);
    for(std::size_t i=0;i<d.fields.size();++i)if(y==DialogFieldRow(r,i)&&x>=inputCol&&x<inputCol+inputWidth){d.focusedControl=i;return;}
    const int buttonRow=r.top+r.height-3,submitCol=r.left+r.width/2-14,cancelCol=r.left+r.width/2+3;
    if(y==buttonRow&&x>=submitCol&&x<submitCol+12)SubmitCommandDialog();else if(y==buttonRow&&x>=cancelCol&&x<cancelCol+12)CloseCommandDialog();
}

void Application::MoveDialogFocus(int delta){if(!commandDialog_)return;auto& d=*commandDialog_;const std::size_t count=d.fields.size()+2;d.focusedControl=delta<0?(d.focusedControl==0?count-1:d.focusedControl-1):(d.focusedControl+1)%count;}

bool Application::SubmitCommandDialog() {
    if(!commandDialog_)return false;for(const auto& f:commandDialog_->fields)if(f.value.empty()){commandDialog_->validationMessage="Required field is empty: "+f.label;return false;}
    const std::string line=BuildDialogCommand();if(line.empty())return false;CloseCommandDialog();commandProcessor_.Execute(line);AddCommandToHistory(line);persistenceStatus_.clear();return true;
}

std::string Application::BuildDialogCommand() const {
    if(!commandDialog_)return{};const auto& d=*commandDialog_;
    if(d.command=="color"&&d.fields.size()==3){const auto& target=d.fields[0].value;const auto& fg=d.fields[1].value;const auto& bg=d.fields[2].value;if(target=="default")return "color "+QuoteArgument(fg)+" bg "+QuoteArgument(bg);return "color "+QuoteArgument(target)+" "+QuoteArgument(fg)+" bg "+QuoteArgument(bg);}
    std::string result=d.command;for(const auto& field:d.fields)result+=" "+QuoteArgument(field.value);return result;
}

std::vector<std::string> Application::TaskIdOptions() const {std::vector<std::string> result;for(const auto& task:taskManager_.GetTasks())result.push_back(task.GetId());return result;}
std::vector<std::string> Application::ColorNameOptions() const {std::vector<std::string> result{"black","red","green","yellow","blue","magenta","cyan","white","brightBlack","brightRed","brightGreen","brightYellow","brightBlue","brightMagenta","brightCyan","brightWhite"};for(const auto& [name,color]:taskManager_.GetDefinedColors()){(void)color;result.push_back(name);}std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;}
std::string Application::QuoteArgument(const std::string& value){return QuoteIfNeeded(value);}

void Application::Render() {
    erase();attr_set(A_NORMAL,0,nullptr);const auto suggestions=commandDialog_?std::vector<Suggestion>{}:BuildSuggestions();if(!suggestions.empty()&&selectedSuggestion_>=suggestions.size())selectedSuggestion_=0;
    RenderHeader();RenderTasks();if(!commandDialog_)RenderSuggestions(suggestions);RenderStatus();RenderCommandLine();if(commandDialog_)RenderCommandDialog();refresh();
}

void Application::RenderHeader(){attr_set(A_NORMAL,0,nullptr);int h=0,w=0;getmaxyx(stdscr,h,w);(void)h;const std::string title=commandDialog_?"Sentinel - Productivity Tracker | COMMAND ARGUMENT WINDOW":"Sentinel - Productivity Tracker | "+taskManager_.GetJsonFile();mvaddnstr(0,0,title.c_str(),std::max(0,w-1));if(w>1)mvhline(1,0,ACS_HLINE,w-1);}

std::vector<Application::Suggestion> Application::BuildSuggestions() const {
    std::vector<Suggestion> suggestions;if(commandBuffer_.empty())return suggestions;const auto separator=commandBuffer_.find_first_of(" \t");const std::string command=commandBuffer_.substr(0,separator);
    if(separator==std::string::npos){struct Ranked{int score;CommandDefinition definition;};std::vector<Ranked> ranked;for(const auto& definition:Commands){const auto score=FuzzySearch::Score(command,definition.name);if(score)ranked.push_back({*score,definition});}std::sort(ranked.begin(),ranked.end(),[](const auto&a,const auto&b){return a.score!=b.score?a.score>b.score:a.definition.name<b.definition.name;});for(std::size_t i=0;i<ranked.size()&&i<MaxSuggestions;++i)suggestions.push_back({std::string(ranked[i].definition.name)+"  -  "+std::string(ranked[i].definition.description),std::string(ranked[i].definition.name)+" "});return suggestions;}
    if(!TakesTaskArgument(command))return suggestions;const std::string query=UnquotePartial(commandBuffer_.substr(separator+1));std::vector<std::size_t> indices;if(query.empty())for(std::size_t i=0;i<taskManager_.GetTasks().size();++i)indices.push_back(i);else indices=taskManager_.Search(query);
    for(const auto index:indices){if(suggestions.size()>=MaxSuggestions)break;const Task* task=taskManager_.GetTask(index);if(!task)continue;std::ostringstream label;label<<task->GetId()<<"  -  "<<task->GetName();if(task->IsCompleted())label<<"  [completed]";else if(task->IsRunning())label<<"  [running]";suggestions.push_back({label.str(),command+" "+QuoteIfNeeded(task->GetId())});}return suggestions;
}

std::vector<std::size_t> Application::VisibleTaskIndices() const {if(commandProcessor_.GetSearchResults())return *commandProcessor_.GetSearchResults();std::vector<std::size_t> result;for(std::size_t i=0;i<taskManager_.GetTasks().size();++i)result.push_back(i);return result;}
int Application::SuggestionStartRow(std::size_t count) const{int h=0,w=0;getmaxyx(stdscr,h,w);(void)w;return std::max(2,h-2-static_cast<int>(count));}

void Application::RenderTasks() {
    int h=0,w=0;getmaxyx(stdscr,h,w);const auto suggestions=commandDialog_?std::vector<Suggestion>{}:BuildSuggestions();const int lastRow=SuggestionStartRow(suggestions.size());int row=2;
    if(!commandProcessor_.GetInfoLines().empty()){attr_set(A_NORMAL,0,nullptr);for(const auto& line:commandProcessor_.GetInfoLines()){if(row>=lastRow)break;mvaddnstr(row++,0,line.c_str(),std::max(0,w-1));}return;}
    std::size_t slot=0;for(const auto index:VisibleTaskIndices()){if(row>=lastRow)break;const Task* task=taskManager_.GetTask(index);if(!task)continue;const char* state=task->IsCompleted()?"[x]":task->IsRunning()?"[>]":"[ ]";const std::string idField=task->GetId()+" "+state;const std::string nameField=task->GetName();const std::string timeField=FormatDuration(task->GetElapsedTime())+"  "+task->GetCompletionDateString();const int rightEdge=std::max(0,w-1),nameColumn=std::clamp(w/4,8,std::max(8,rightEdge)),timeWidth=static_cast<int>(timeField.size()),desiredTimeColumn=rightEdge-timeWidth,minimumTimeColumn=nameColumn+8,timeColumn=std::max(minimumTimeColumn,desiredTimeColumn),idWidth=std::max(0,nameColumn-2),nameWidth=std::max(0,std::min(timeColumn-nameColumn-2,rightEdge-nameColumn)),actualTimeColumn=std::min(timeColumn,std::max(0,rightEdge-timeWidth)),actualTimeWidth=std::max(0,rightEdge-actualTimeColumn);short pair=TaskColorPair;
        if(has_colors()&&task->HasCustomColor()){const short candidate=static_cast<short>(FirstPerTaskPair+slot);if(candidate>0&&candidate<COLOR_PAIRS&&InitializeTaskColorPair(candidate,*task->GetForegroundColor(),*task->GetBackgroundColor()))pair=candidate;}if(has_colors())attr_set(A_NORMAL,pair,nullptr);move(row,0);clrtoeol();DrawClippedField(row,0,idWidth,idField);DrawClippedField(row,nameColumn,nameWidth,nameField);DrawClippedField(row,actualTimeColumn,actualTimeWidth,timeField);++row;attr_set(A_NORMAL,0,nullptr);++slot;}
}

void Application::RenderSuggestions(const std::vector<Suggestion>& suggestions){if(suggestions.empty())return;attr_set(A_NORMAL,0,nullptr);int h=0,w=0;getmaxyx(stdscr,h,w);(void)h;const int start=SuggestionStartRow(suggestions.size());for(std::size_t i=0;i<suggestions.size();++i){const bool selected=i==selectedSuggestion_;if(selected)attron(A_REVERSE);const std::string line=(selected?"> ":"  ")+suggestions[i].label;mvaddnstr(start+static_cast<int>(i),0,line.c_str(),std::max(0,w-1));if(selected)attroff(A_REVERSE);}}

void Application::RenderStatus(){attr_set(A_NORMAL,0,nullptr);int h=0,w=0;getmaxyx(stdscr,h,w);if(h<2)return;std::string status=commandProcessor_.GetStatusMessage();if(!persistenceStatus_.empty()){status+=status.empty()?"":" | ";status+=persistenceStatus_;}if(commandDialog_){status+=status.empty()?"":" | ";status+="Tab next | Shift+Tab previous | Enter choose | F2 submit | Esc cancel";}else if(!BuildSuggestions().empty()){status+=status.empty()?"":" | ";status+="Tab complete | Down suggestions | Up history | Left/Right cursor | click select";}else if(!commandHistory_.empty()){status+=status.empty()?"":" | ";status+="Up: previous command | Left/Right: cursor";}mvaddnstr(h-2,0,status.c_str(),std::max(0,w-1));}

void Application::RenderCommandLine(){attr_set(A_NORMAL,0,nullptr);int h=0,w=0;getmaxyx(stdscr,h,w);if(h<1||w<1)return;const int row=h-1;move(row,0);clrtoeol();if(commandDialog_){curs_set(0);const std::string line="> [argument window: "+commandDialog_->command+"]";mvaddnstr(row,0,line.c_str(),std::max(0,w-1));return;}curs_set(1);mvaddnstr(row,0,"> ",std::max(0,w-1));if(w>2)mvaddnstr(row,2,commandBuffer_.c_str(),w-3);move(row,std::min(w-1,static_cast<int>(cursorPosition_)+2));}

void Application::RenderCommandDialog() {
    if(!commandDialog_)return;attr_set(A_NORMAL,0,nullptr);auto& d=*commandDialog_;const auto r=CalculateDialogRect(d.fields.size());
    for(int y=r.top;y<r.top+r.height;++y){move(y,r.left);for(int x=0;x<r.width;++x)addch(' ');}mvhline(r.top,r.left+1,ACS_HLINE,r.width-2);mvhline(r.top+r.height-1,r.left+1,ACS_HLINE,r.width-2);mvvline(r.top+1,r.left,ACS_VLINE,r.height-2);mvvline(r.top+1,r.left+r.width-1,ACS_VLINE,r.height-2);mvaddch(r.top,r.left,ACS_ULCORNER);mvaddch(r.top,r.left+r.width-1,ACS_URCORNER);mvaddch(r.top+r.height-1,r.left,ACS_LLCORNER);mvaddch(r.top+r.height-1,r.left+r.width-1,ACS_LRCORNER);
    DrawClippedField(r.top+1,r.left+3,r.width-6,d.title);DrawClippedField(r.top+2,r.left+3,r.width-6,"Fill arguments manually. CLI syntax remains supported.");const int labelWidth=std::min(18,std::max(10,r.width/4)),inputCol=r.left+3+labelWidth,inputWidth=std::max(10,r.width-labelWidth-7);
    for(std::size_t i=0;i<d.fields.size();++i){auto& f=d.fields[i];const int row=DialogFieldRow(r,i);DrawClippedField(row,r.left+3,labelWidth-1,f.label+":");if(d.focusedControl==i)attron(A_REVERSE);const std::string value=f.value.empty()&&f.kind==DialogFieldKind::DropList?"(no options)":f.value;DrawClippedField(row,inputCol,inputWidth,"[ "+value+(f.kind==DialogFieldKind::DropList?"  v":"")+" ]");if(d.focusedControl==i)attroff(A_REVERSE);}
    const std::size_t submit=d.fields.size(),cancel=submit+1;const int buttonRow=r.top+r.height-3,submitCol=r.left+r.width/2-14,cancelCol=r.left+r.width/2+3;if(d.focusedControl==submit)attron(A_REVERSE);DrawClippedField(buttonRow,submitCol,12,"[ Submit ]");if(d.focusedControl==submit)attroff(A_REVERSE);if(d.focusedControl==cancel)attron(A_REVERSE);DrawClippedField(buttonRow,cancelCol,12,"[ Cancel ]");if(d.focusedControl==cancel)attroff(A_REVERSE);DrawClippedField(r.top+r.height-2,r.left+3,r.width-6,d.validationMessage);
    if(d.focusedControl<d.fields.size()&&d.fields[d.focusedControl].kind==DialogFieldKind::TextInput){curs_set(1);move(DialogFieldRow(r,d.focusedControl),std::min(inputCol+inputWidth-2,inputCol+2+static_cast<int>(d.fields[d.focusedControl].cursor)));}else curs_set(0);
}

std::string Application::FormatDuration(std::chrono::seconds duration){const auto total=duration.count();std::ostringstream out;out<<std::setfill('0')<<std::setw(2)<<total/3600<<':'<<std::setw(2)<<(total%3600)/60<<':'<<std::setw(2)<<total%60;return out.str();}

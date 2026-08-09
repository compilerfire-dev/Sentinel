#include "Application.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {

struct PlotBounds {
    double left{72.0};
    double top{28.0};
    double right{24.0};
    double bottom{54.0};
};

std::string FormatDuration(std::uint64_t seconds) {
    const auto hours = seconds / 3600;
    const auto minutes = (seconds % 3600) / 60;
    std::ostringstream stream;
    stream << hours << "h " << minutes << "m";
    return stream.str();
}

std::string FormatDate(std::int64_t epoch) {
    const std::time_t value = static_cast<std::time_t>(epoch);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &value);
#else
    localtime_r(&value, &local);
#endif
    std::ostringstream stream;
    stream << std::put_time(&local, "%Y-%m-%d");
    return stream.str();
}

void SetSourceHex(cairo_t* cr, double r, double g, double b) {
    cairo_set_source_rgb(cr, r, g, b);
}

void DrawText(cairo_t* cr, double x, double y, const std::string& text, double size = 12.0) {
    cairo_set_font_size(cr, size);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text.c_str());
}

void DrawEmptyMessage(cairo_t* cr, int width, int height, const std::string& message) {
    SetSourceHex(cr, 0.45, 0.45, 0.45);
    cairo_set_font_size(cr, 14.0);
    cairo_text_extents_t extents{};
    cairo_text_extents(cr, message.c_str(), &extents);
    cairo_move_to(cr, std::max(12.0, (width - extents.width) / 2.0), height / 2.0);
    cairo_show_text(cr, message.c_str());
}

void DrawAxes(
    cairo_t* cr,
    int width,
    int height,
    const PlotBounds& bounds,
    double maxY,
    std::int64_t minX,
    std::int64_t maxX,
    const std::string& yLabel
) {
    const double x0 = bounds.left;
    const double y0 = height - bounds.bottom;
    const double x1 = width - bounds.right;
    const double y1 = bounds.top;

    SetSourceHex(cr, 0.35, 0.35, 0.35);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, x0, y1);
    cairo_line_to(cr, x0, y0);
    cairo_line_to(cr, x1, y0);
    cairo_stroke(cr);

    const int gridLines = 5;
    for (int index = 0; index <= gridLines; ++index) {
        const double ratio = static_cast<double>(index) / gridLines;
        const double y = y0 - ratio * (y0 - y1);
        const double value = ratio * maxY;

        SetSourceHex(cr, 0.88, 0.88, 0.88);
        cairo_move_to(cr, x0, y);
        cairo_line_to(cr, x1, y);
        cairo_stroke(cr);

        SetSourceHex(cr, 0.25, 0.25, 0.25);
        std::ostringstream label;
        label << static_cast<long long>(std::llround(value));
        DrawText(cr, 10, y + 4, label.str(), 11.0);
    }

    SetSourceHex(cr, 0.25, 0.25, 0.25);
    DrawText(cr, x0, height - 18, FormatDate(minX), 11.0);
    const std::string last = FormatDate(maxX);
    cairo_text_extents_t extents{};
    cairo_set_font_size(cr, 11.0);
    cairo_text_extents(cr, last.c_str(), &extents);
    DrawText(cr, x1 - extents.width, height - 18, last, 11.0);
    DrawText(cr, 10, 18, yLabel, 11.0);
}

std::pair<std::int64_t, std::int64_t> XRange(const std::vector<TimePointValue>& points) {
    if (points.empty()) return {0, 0};
    return {points.front().epochSeconds, points.back().epochSeconds};
}

void DrawLineSeries(
    cairo_t* cr,
    const std::vector<TimePointValue>& points,
    int width,
    int height,
    const PlotBounds& bounds,
    std::int64_t minX,
    std::int64_t maxX,
    double maxY,
    double red,
    double green,
    double blue
) {
    if (points.empty()) return;

    const double plotWidth = std::max(1.0, width - bounds.left - bounds.right);
    const double plotHeight = std::max(1.0, height - bounds.top - bounds.bottom);
    const double xSpan = std::max<std::int64_t>(1, maxX - minX);
    maxY = std::max(1.0, maxY);

    SetSourceHex(cr, red, green, blue);
    cairo_set_line_width(cr, 2.2);

    bool first = true;
    for (const auto& point : points) {
        const double x = bounds.left + (point.epochSeconds - minX) / xSpan * plotWidth;
        const double y = bounds.top + (1.0 - point.value / maxY) * plotHeight;
        if (first) {
            cairo_move_to(cr, x, y);
            first = false;
        } else {
            cairo_line_to(cr, x, y);
        }
    }
    cairo_stroke(cr);

    for (const auto& point : points) {
        const double x = bounds.left + (point.epochSeconds - minX) / xSpan * plotWidth;
        const double y = bounds.top + (1.0 - point.value / maxY) * plotHeight;
        cairo_arc(cr, x, y, 3.0, 0.0, 2.0 * 3.14159265358979323846);
        cairo_fill(cr);
    }
}

const std::array<std::array<double, 3>, 8> SeriesColors{{
    {{0.18, 0.49, 0.79}},
    {{0.86, 0.33, 0.25}},
    {{0.20, 0.62, 0.42}},
    {{0.58, 0.39, 0.75}},
    {{0.90, 0.61, 0.18}},
    {{0.25, 0.67, 0.72}},
    {{0.66, 0.42, 0.24}},
    {{0.50, 0.50, 0.50}}
}};

GtkWidget* CreateMetric(const char* title, GtkWidget** valueLabel) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* titleLabel = gtk_label_new(title);
    *valueLabel = gtk_label_new("0");
    gtk_widget_set_halign(titleLabel, GTK_ALIGN_START);
    gtk_widget_set_halign(*valueLabel, GTK_ALIGN_START);

    PangoAttrList* attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_scale_new(1.5));
    gtk_label_set_attributes(GTK_LABEL(*valueLabel), attrs);
    pango_attr_list_unref(attrs);

    gtk_box_pack_start(GTK_BOX(box), titleLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), *valueLabel, FALSE, FALSE, 0);
    return box;
}

} // namespace

int Application::Run(int argc, char** argv) {
    if (argc > 1 && argv[1] && *argv[1]) initialPath_ = argv[1];

    GtkApplication* app = gtk_application_new(
        "dev.compilerfire.sentinel.stats",
        G_APPLICATION_FLAGS_NONE
    );
    g_signal_connect(app, "activate", G_CALLBACK(OnActivate), this);

    // SentinelStats consumes the optional JSON path itself. Passing that path
    // to g_application_run() would make GApplication interpret it as a file
    // open request, which requires G_APPLICATION_HANDLES_OPEN and an "open"
    // signal handler. Run GTK with only argv[0] instead.
    char* gtkArgv[] = { (argc > 0 && argv && argv[0]) ? argv[0] : const_cast<char*>("SentinelStats"), nullptr };
    const int result = g_application_run(G_APPLICATION(app), 1, gtkArgv);

    g_object_unref(app);
    return result;
}

void Application::OnActivate(GtkApplication* app, gpointer userData) {
    static_cast<Application*>(userData)->BuildWindow(app);
}

void Application::OnOpenClicked(GtkButton*, gpointer userData) {
    static_cast<Application*>(userData)->OpenJsonFile();
}

void Application::OnReloadClicked(GtkButton*, gpointer userData) {
    static_cast<Application*>(userData)->Reload();
}

gboolean Application::OnTasksDraw(GtkWidget* widget, cairo_t* cr, gpointer userData) {
    static_cast<Application*>(userData)->DrawCompletedTasks(widget, cr);
    return FALSE;
}

gboolean Application::OnLocDraw(GtkWidget* widget, cairo_t* cr, gpointer userData) {
    static_cast<Application*>(userData)->DrawProjectLoc(widget, cr);
    return FALSE;
}

void Application::BuildWindow(GtkApplication* app) {
    window_ = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window_), "SentinelStats");
    gtk_window_set_default_size(GTK_WINDOW(window_), 1400, 900);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(root), 16);
    gtk_container_add(GTK_CONTAINER(window_), root);

    GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* openButton = gtk_button_new_with_label("Open JSON");
    GtkWidget* reloadButton = gtk_button_new_with_label("Reload");
    fileLabel_ = gtk_label_new("");
    gtk_widget_set_halign(fileLabel_, GTK_ALIGN_START);
    gtk_widget_set_hexpand(fileLabel_, TRUE);

    gtk_box_pack_start(GTK_BOX(toolbar), openButton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), reloadButton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), fileLabel_, TRUE, TRUE, 8);
    gtk_box_pack_start(GTK_BOX(root), toolbar, FALSE, FALSE, 0);

    g_signal_connect(openButton, "clicked", G_CALLBACK(OnOpenClicked), this);
    g_signal_connect(reloadButton, "clicked", G_CALLBACK(OnReloadClicked), this);

    GtkWidget* metrics = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 36);
    gtk_box_pack_start(GTK_BOX(metrics), CreateMetric("Total tasks", &totalTasksLabel_), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(metrics), CreateMetric("Completed", &completedTasksLabel_), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(metrics), CreateMetric("Tracked time", &trackedTimeLabel_), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(metrics), CreateMetric("Projects", &projectCountLabel_), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(root), metrics, FALSE, FALSE, 4);

    GtkWidget* notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(notebook, TRUE);
    gtk_box_pack_start(GTK_BOX(root), notebook, TRUE, TRUE, 0);

    tasksDrawingArea_ = gtk_drawing_area_new();
    gtk_widget_set_size_request(tasksDrawingArea_, -1, 520);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tasksDrawingArea_, gtk_label_new("Task progression"));
    g_signal_connect(tasksDrawingArea_, "draw", G_CALLBACK(OnTasksDraw), this);

    locDrawingArea_ = gtk_drawing_area_new();
    gtk_widget_set_size_request(locDrawingArea_, -1, 520);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), locDrawingArea_, gtk_label_new("Lines of code"));
    g_signal_connect(locDrawingArea_, "draw", G_CALLBACK(OnLocDraw), this);

    statusLabel_ = gtk_label_new("");
    gtk_widget_set_halign(statusLabel_, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(root), statusLabel_, FALSE, FALSE, 0);

    LoadPath(initialPath_);
    gtk_widget_show_all(window_);
}

void Application::OpenJsonFile() {
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Open Sentinel JSON",
        GTK_WINDOW(window_),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT,
        nullptr
    );

    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "JSON files");
    gtk_file_filter_add_pattern(filter, "*.json");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            LoadPath(filename);
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
}

void Application::Reload() {
    if (data_.Path().empty()) {
        LoadPath(initialPath_);
        return;
    }
    LoadPath(data_.Path());
}

bool Application::LoadPath(const std::filesystem::path& path) {
    std::string error;
    if (!data_.Load(path, error)) {
        if (statusLabel_) gtk_label_set_text(GTK_LABEL(statusLabel_), error.c_str());
        if (fileLabel_) gtk_label_set_text(GTK_LABEL(fileLabel_), path.string().c_str());
        return false;
    }

    RefreshLabels();
    QueueCharts();
    return true;
}

void Application::RefreshLabels() {
    const auto& snapshot = data_.Snapshot();
    gtk_label_set_text(GTK_LABEL(fileLabel_), data_.Path().string().c_str());
    gtk_label_set_text(GTK_LABEL(totalTasksLabel_), std::to_string(snapshot.totalTasks).c_str());

    std::ostringstream completed;
    completed << snapshot.completedTasks << " / " << snapshot.totalTasks;
    gtk_label_set_text(GTK_LABEL(completedTasksLabel_), completed.str().c_str());
    gtk_label_set_text(GTK_LABEL(trackedTimeLabel_), FormatDuration(snapshot.totalTrackedSeconds).c_str());
    gtk_label_set_text(GTK_LABEL(projectCountLabel_), std::to_string(snapshot.projects.size()).c_str());

    std::ostringstream status;
    status << "Loaded " << snapshot.totalTasks << " tasks; "
           << snapshot.completedTaskHistory.size() << " timestamped completions; "
           << snapshot.projects.size() << " project series.";
    gtk_label_set_text(GTK_LABEL(statusLabel_), status.str().c_str());
}

void Application::QueueCharts() {
    if (tasksDrawingArea_) gtk_widget_queue_draw(tasksDrawingArea_);
    if (locDrawingArea_) gtk_widget_queue_draw(locDrawingArea_);
}

void Application::DrawCompletedTasks(GtkWidget* widget, cairo_t* cr) const {
    const GtkAllocation allocation = [&]() {
        GtkAllocation value{};
        gtk_widget_get_allocation(widget, &value);
        return value;
    }();

    const auto& points = data_.Snapshot().completedTaskHistory;
    SetSourceHex(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    SetSourceHex(cr, 0.12, 0.12, 0.12);
    DrawText(cr, 18, 22, "Cumulative completed tasks", 16.0);

    if (points.empty()) {
        DrawEmptyMessage(cr, allocation.width, allocation.height, "No timestamped completed tasks in this JSON file.");
        return;
    }

    const auto [minX, maxX] = XRange(points);
    double maxY = 1.0;
    for (const auto& point : points) maxY = std::max(maxY, point.value);

    const PlotBounds bounds;
    DrawAxes(cr, allocation.width, allocation.height, bounds, maxY, minX, maxX, "Completed tasks");
    DrawLineSeries(cr, points, allocation.width, allocation.height, bounds, minX, maxX, maxY, 0.18, 0.49, 0.79);
}

void Application::DrawProjectLoc(GtkWidget* widget, cairo_t* cr) const {
    GtkAllocation allocation{};
    gtk_widget_get_allocation(widget, &allocation);

    SetSourceHex(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);
    SetSourceHex(cr, 0.12, 0.12, 0.12);
    DrawText(cr, 18, 22, "Project lines of code over time", 16.0);

    std::vector<const ProjectSeries*> projects;
    for (const auto& project : data_.Snapshot().projects) {
        if (!project.locHistory.empty()) projects.push_back(&project);
    }
    if (projects.empty()) {
        DrawEmptyMessage(cr, allocation.width, allocation.height, "No project loc_history data in this JSON file.");
        return;
    }

    std::int64_t minX = projects.front()->locHistory.front().epochSeconds;
    std::int64_t maxX = projects.front()->locHistory.back().epochSeconds;
    double maxY = 1.0;
    for (const auto* project : projects) {
        minX = std::min(minX, project->locHistory.front().epochSeconds);
        maxX = std::max(maxX, project->locHistory.back().epochSeconds);
        for (const auto& point : project->locHistory) maxY = std::max(maxY, point.value);
    }

    PlotBounds bounds;
    bounds.top = 48.0;
    DrawAxes(cr, allocation.width, allocation.height, bounds, maxY, minX, maxX, "Lines of code");

    for (std::size_t index = 0; index < projects.size(); ++index) {
        const auto color = SeriesColors[index % SeriesColors.size()];
        DrawLineSeries(
            cr,
            projects[index]->locHistory,
            allocation.width,
            allocation.height,
            bounds,
            minX,
            maxX,
            maxY,
            color[0], color[1], color[2]
        );

        const double legendX = 18.0 + (index % 4) * 210.0;
        const double legendY = 40.0 + (index / 4) * 18.0;
        SetSourceHex(cr, color[0], color[1], color[2]);
        cairo_rectangle(cr, legendX, legendY - 9, 12, 3);
        cairo_fill(cr);
        SetSourceHex(cr, 0.15, 0.15, 0.15);
        DrawText(cr, legendX + 18, legendY, projects[index]->name, 11.0);
    }
}

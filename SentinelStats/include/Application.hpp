#pragma once

#include "StatisticsData.hpp"

#include <gtk/gtk.h>

#include <filesystem>
#include <string>

class Application {
public:
    int Run(int argc, char** argv);

private:
    static void OnActivate(GtkApplication* app, gpointer userData);
    static void OnOpenClicked(GtkButton* button, gpointer userData);
    static void OnReloadClicked(GtkButton* button, gpointer userData);
    static gboolean OnTasksDraw(GtkWidget* widget, cairo_t* cr, gpointer userData);
    static gboolean OnLocDraw(GtkWidget* widget, cairo_t* cr, gpointer userData);

    void BuildWindow(GtkApplication* app);
    void OpenJsonFile();
    void Reload();
    bool LoadPath(const std::filesystem::path& path);
    void RefreshLabels();
    void QueueCharts();

    void DrawCompletedTasks(GtkWidget* widget, cairo_t* cr) const;
    void DrawProjectLoc(GtkWidget* widget, cairo_t* cr) const;

    StatisticsData data_;
    std::filesystem::path initialPath_{"current_data.json"};

    GtkWidget* window_{nullptr};
    GtkWidget* fileLabel_{nullptr};
    GtkWidget* statusLabel_{nullptr};
    GtkWidget* totalTasksLabel_{nullptr};
    GtkWidget* completedTasksLabel_{nullptr};
    GtkWidget* trackedTimeLabel_{nullptr};
    GtkWidget* projectCountLabel_{nullptr};
    GtkWidget* tasksDrawingArea_{nullptr};
    GtkWidget* locDrawingArea_{nullptr};
};

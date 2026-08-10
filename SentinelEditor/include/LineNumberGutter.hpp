#pragma once

#include <gtk/gtk.h>

class LineNumberGutter {
public:
    void Attach(GtkTextView* textView, GtkScrolledWindow* scrolledWindow);

    GtkWidget* Widget() const noexcept;
    void QueueDraw();

private:
    static gboolean OnDraw(GtkWidget* widget, cairo_t* cr, gpointer userData);
    static void OnAdjustmentValueChanged(GtkAdjustment* adjustment, gpointer userData);
    static void OnBufferChanged(GtkTextBuffer* buffer, gpointer userData);
    static void OnBufferMarkSet(
        GtkTextBuffer* buffer,
        GtkTextIter* location,
        GtkTextMark* mark,
        gpointer userData
    );
    static void OnTextViewSizeAllocate(
        GtkWidget* widget,
        GtkAllocation* allocation,
        gpointer userData
    );

    void Draw(GtkWidget* widget, cairo_t* cr);
    void UpdateWidth();

    GtkTextView* textView_{nullptr};
    GtkTextBuffer* textBuffer_{nullptr};
    GtkWidget* drawingArea_{nullptr};
    GtkAdjustment* verticalAdjustment_{nullptr};
};

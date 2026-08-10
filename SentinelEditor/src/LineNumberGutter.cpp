#include "LineNumberGutter.hpp"

#include <algorithm>
#include <pango/pangocairo.h>
#include <string>

void LineNumberGutter::Attach(
    GtkTextView* textView,
    GtkScrolledWindow* scrolledWindow
) {
    textView_ = textView;
    textBuffer_ = textView_ ? gtk_text_view_get_buffer(textView_) : nullptr;
    verticalAdjustment_ = scrolledWindow
        ? gtk_scrolled_window_get_vadjustment(scrolledWindow)
        : nullptr;

    drawingArea_ = gtk_drawing_area_new();
    gtk_widget_set_vexpand(drawingArea_, TRUE);
    gtk_widget_set_can_focus(drawingArea_, FALSE);

    g_signal_connect(
        drawingArea_,
        "draw",
        G_CALLBACK(OnDraw),
        this
    );

    if (verticalAdjustment_) {
        g_signal_connect(
            verticalAdjustment_,
            "value-changed",
            G_CALLBACK(OnAdjustmentValueChanged),
            this
        );
    }

    if (textBuffer_) {
        g_signal_connect(
            textBuffer_,
            "changed",
            G_CALLBACK(OnBufferChanged),
            this
        );
        g_signal_connect(
            textBuffer_,
            "mark-set",
            G_CALLBACK(OnBufferMarkSet),
            this
        );
    }

    if (textView_) {
        g_signal_connect(
            textView_,
            "size-allocate",
            G_CALLBACK(OnTextViewSizeAllocate),
            this
        );
    }

    UpdateWidth();
}

GtkWidget* LineNumberGutter::Widget() const noexcept {
    return drawingArea_;
}

void LineNumberGutter::QueueDraw() {
    if (drawingArea_) gtk_widget_queue_draw(drawingArea_);
}

gboolean LineNumberGutter::OnDraw(
    GtkWidget* widget,
    cairo_t* cr,
    gpointer userData
) {
    static_cast<LineNumberGutter*>(userData)->Draw(widget, cr);
    return FALSE;
}

void LineNumberGutter::OnAdjustmentValueChanged(
    GtkAdjustment*,
    gpointer userData
) {
    static_cast<LineNumberGutter*>(userData)->QueueDraw();
}

void LineNumberGutter::OnBufferChanged(
    GtkTextBuffer*,
    gpointer userData
) {
    auto* self = static_cast<LineNumberGutter*>(userData);
    self->UpdateWidth();
    self->QueueDraw();
}

void LineNumberGutter::OnBufferMarkSet(
    GtkTextBuffer* buffer,
    GtkTextIter*,
    GtkTextMark* mark,
    gpointer userData
) {
    if (mark != gtk_text_buffer_get_insert(buffer)) return;
    static_cast<LineNumberGutter*>(userData)->QueueDraw();
}

void LineNumberGutter::OnTextViewSizeAllocate(
    GtkWidget*,
    GtkAllocation*,
    gpointer userData
) {
    static_cast<LineNumberGutter*>(userData)->QueueDraw();
}

void LineNumberGutter::Draw(GtkWidget* widget, cairo_t* cr) {
    if (!widget || !cr || !textView_ || !textBuffer_) return;

    GtkAllocation allocation{};
    gtk_widget_get_allocation(widget, &allocation);

    GtkStyleContext* style = gtk_widget_get_style_context(widget);
    const GtkStateFlags state = gtk_widget_get_state_flags(widget);
    gtk_render_background(
        style,
        cr,
        0.0,
        0.0,
        static_cast<double>(allocation.width),
        static_cast<double>(allocation.height)
    );

    GdkRGBA foreground{};
    gtk_style_context_get_color(style, state, &foreground);

    GdkRectangle visible{};
    gtk_text_view_get_visible_rect(textView_, &visible);

    GtkTextIter current;
    gtk_text_buffer_get_iter_at_mark(
        textBuffer_,
        &current,
        gtk_text_buffer_get_insert(textBuffer_)
    );
    const int currentLine = gtk_text_iter_get_line(&current);

    GtkTextIter iter;
    gint lineTop = 0;
    gtk_text_view_get_line_at_y(textView_, &iter, visible.y, &lineTop);

    while (true) {
        const int line = gtk_text_iter_get_line(&iter);
        gint lineY = 0;
        gint lineHeight = 0;
        gtk_text_view_get_line_yrange(textView_, &iter, &lineY, &lineHeight);

        if (lineY >= visible.y + visible.height) break;

        const double screenY = static_cast<double>(lineY - visible.y);
        const double height = static_cast<double>(std::max(1, lineHeight));
        const bool isCurrent = line == currentLine;

        if (isCurrent) {
            cairo_set_source_rgba(
                cr,
                foreground.red,
                foreground.green,
                foreground.blue,
                0.10
            );
            cairo_rectangle(
                cr,
                0.0,
                screenY,
                static_cast<double>(allocation.width),
                height
            );
            cairo_fill(cr);
        }

        const std::string number = std::to_string(line + 1);
        PangoLayout* layout = gtk_widget_create_pango_layout(widget, number.c_str());
        PangoFontDescription* font = pango_font_description_from_string("Monospace 10");
        pango_layout_set_font_description(layout, font);
        pango_font_description_free(font);

        int textWidth = 0;
        int textHeight = 0;
        pango_layout_get_pixel_size(layout, &textWidth, &textHeight);

        const double x = std::max(
            4.0,
            static_cast<double>(allocation.width - textWidth - 9)
        );
        const double y = screenY + std::max(
            0.0,
            (height - static_cast<double>(textHeight)) / 2.0
        );

        cairo_set_source_rgba(
            cr,
            foreground.red,
            foreground.green,
            foreground.blue,
            isCurrent ? 1.0 : 0.58
        );
        cairo_move_to(cr, x, y);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);

        if (!gtk_text_iter_forward_line(&iter)) break;
    }

    cairo_set_source_rgba(
        cr,
        foreground.red,
        foreground.green,
        foreground.blue,
        0.18
    );
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, allocation.width - 0.5, 0.0);
    cairo_line_to(cr, allocation.width - 0.5, allocation.height);
    cairo_stroke(cr);
}

void LineNumberGutter::UpdateWidth() {
    if (!drawingArea_ || !textBuffer_) return;

    const int lineCount = std::max(1, gtk_text_buffer_get_line_count(textBuffer_));
    const int digits = static_cast<int>(std::to_string(lineCount).size());

    PangoLayout* layout = gtk_widget_create_pango_layout(drawingArea_, "0");
    PangoFontDescription* font = pango_font_description_from_string("Monospace 10");
    pango_layout_set_font_description(layout, font);
    pango_font_description_free(font);

    int digitWidth = 8;
    int digitHeight = 0;
    pango_layout_get_pixel_size(layout, &digitWidth, &digitHeight);
    (void)digitHeight;
    g_object_unref(layout);

    const int width = std::max(42, 18 + digits * std::max(1, digitWidth));
    gtk_widget_set_size_request(drawingArea_, width, -1);
}

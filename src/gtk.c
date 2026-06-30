#include "lil.h"

#ifdef HAVE_GTK

static GtkW gtk_ws[MAX_GTK_WIDGETS];
static int gtk_wc;
static int gtk_window_gone;
static int gtk_in_main;

GtkWidget *gtk_find_w(const char *name)
{
    for (int i = 0; i < gtk_wc; i++) if (!strcmp(gtk_ws[i].name, name)) return gtk_ws[i].widget;
    return NULL;
}

void gtk_reg_w(const char *name, GtkWidget *w) {
    if (gtk_wc >= MAX_GTK_WIDGETS) { fprintf(stderr, "too many gtk widgets\n"); return; }
    strncpy(gtk_ws[gtk_wc].name, name, 63); gtk_ws[gtk_wc].name[63] = 0;
    gtk_ws[gtk_wc].widget = w; gtk_wc++;
}

void gtk_destroy_cb(GtkWidget *w, gpointer d) {
    (void)d;
    for (int i = 0; i < gtk_wc; i++) {
        if (gtk_ws[i].widget == w) {
            gtk_ws[i].widget = NULL;
            gtk_ws[i].name[0] = 0;
            break;
        }
    }
    gtk_window_gone = 1;
    if (gtk_in_main) gtk_main_quit();
}

void gtk_sig_cb(GtkWidget *w, gpointer d) {
    GtkSigData *sd = (GtkSigData *)d;
    if (!strcmp(sd->sig, "changed")) {
        if (!GTK_IS_ENTRY(w)) { var_set(sd->vname, make_str("")); return; }
        var_set(sd->vname, make_str(gtk_entry_get_text(GTK_ENTRY(w))));
    } else
        var_set(sd->vname, make_str("1"));
    if (gtk_in_main && strcmp(sd->vname, "__gtk_quit"))
        gtk_main_quit();
}

static gboolean gtk_timeout_cb(gpointer d) {
    GtkSigData *sd = (GtkSigData *)d;
    var_set(sd->vname, make_str("1"));
    if (gtk_in_main) gtk_main_quit();
    return FALSE;
}

Value gtk_dispatch(const char *fn, int argc, char **args, int line) {
    if (!strcmp(fn, "init")) {
        gtk_init_check(NULL, NULL);
        return make_num(0);
    }

    if (!strcmp(fn, "window")) {
        if (argc < 5) fatal("line %d: &gtk|window name title width height", line);
        char *name = resolve_arg(args[1]);
        char *title = resolve_arg(args[2]);
        int w = atoi(resolve_arg(args[3]));
        int h = atoi(resolve_arg(args[4]));
        gtk_init_check(NULL, NULL);
        GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(win), title);
        gtk_window_set_default_size(GTK_WINDOW(win), w, h);
        g_signal_connect(win, "destroy", G_CALLBACK(gtk_destroy_cb), NULL);
        gtk_reg_w(name, win);
        free(name); free(title);
        return make_num(0);
    }

    if (!strcmp(fn, "vbox")) {
        if (argc < 3) fatal("line %d: &gtk|vbox name spacing", line);
        char *name = resolve_arg(args[1]);
        int sp = atoi(resolve_arg(args[2]));
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, sp);
        gtk_reg_w(name, box);
        free(name);
        return make_num(0);
    }

    if (!strcmp(fn, "hbox")) {
        if (argc < 3) fatal("line %d: &gtk|hbox name spacing", line);
        char *name = resolve_arg(args[1]);
        int sp = atoi(resolve_arg(args[2]));
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, sp);
        gtk_reg_w(name, box);
        free(name);
        return make_num(0);
    }

    if (!strcmp(fn, "button")) {
        if (argc < 3) fatal("line %d: &gtk|button name label", line);
        char *name = resolve_arg(args[1]);
        char *label = resolve_arg(args[2]);
        GtkWidget *btn = gtk_button_new_with_label(label);
        gtk_reg_w(name, btn);
        free(name); free(label);
        return make_num(0);
    }

    if (!strcmp(fn, "label")) {
        if (argc < 3) fatal("line %d: &gtk|label name text", line);
        char *name = resolve_arg(args[1]);
        char *text = resolve_arg(args[2]);
        GtkWidget *lbl = gtk_label_new(text);
        gtk_reg_w(name, lbl);
        free(name); free(text);
        return make_num(0);
    }

    if (!strcmp(fn, "entry")) {
        if (argc < 2) fatal("line %d: &gtk|entry name [placeholder]", line);
        char *name = resolve_arg(args[1]);
        GtkWidget *e = gtk_entry_new();
        if (argc >= 3) {
            char *ph = resolve_arg(args[2]);
            gtk_entry_set_placeholder_text(GTK_ENTRY(e), ph);
            free(ph);
        }
        gtk_reg_w(name, e);
        free(name);
        return make_num(0);
    }

    if (!strcmp(fn, "add")) {
        if (argc < 3) fatal("line %d: &gtk|add parent child [child2 ...]", line);
        char *pname = resolve_arg(args[1]);
        GtkWidget *parent = gtk_find_w(pname);
        free(pname);
        if (!parent) fatal("line %d: widget '%s' not found", line, args[1]);
        for (int i = 2; i < argc; i++) {
            char *cname = resolve_arg(args[i]);
            GtkWidget *child = gtk_find_w(cname);
            if (!child) fatal("line %d: widget '%s' not found", line, args[i]);
            gtk_container_add(GTK_CONTAINER(parent), child);
            free(cname);
        }
        return make_num(0);
    }

    if (!strcmp(fn, "show")) {
        if (argc < 2) fatal("line %d: &gtk|show name", line);
        char *name = resolve_arg(args[1]);
        GtkWidget *w = gtk_find_w(name);
        if (!w) fatal("line %d: widget '%s' not found", line, args[1]);
        gtk_widget_show_all(w);
        free(name);
        return make_num(0);
    }

    if (!strcmp(fn, "set")) {
        if (argc < 4) fatal("line %d: &gtk|set widget prop value", line);
        char *name = resolve_arg(args[1]);
        char *prop = resolve_arg(args[2]);
        char *val = resolve_arg(args[3]);
        GtkWidget *w = gtk_find_w(name);
        if (!w) fatal("line %d: widget '%s' not found", line, args[1]);
        if (!strcmp(prop, "label")) {
            if (GTK_IS_BUTTON(w)) gtk_button_set_label(GTK_BUTTON(w), val);
            else if (GTK_IS_LABEL(w)) gtk_label_set_text(GTK_LABEL(w), val);
        } else if (!strcmp(prop, "text")) {
            if (GTK_IS_LABEL(w)) gtk_label_set_text(GTK_LABEL(w), val);
        } else if (!strcmp(prop, "title")) {
            if (GTK_IS_WINDOW(w)) gtk_window_set_title(GTK_WINDOW(w), val);
        } else if (!strcmp(prop, "placeholder")) {
            if (GTK_IS_ENTRY(w)) gtk_entry_set_placeholder_text(GTK_ENTRY(w), val);
        } else if (!strcmp(prop, "sensitive")) {
            gtk_widget_set_sensitive(w, atoi(val));
        } else if (!strcmp(prop, "visible")) {
            gtk_widget_set_visible(w, atoi(val));
        } else if (!strcmp(prop, "width")) {
            int hq;
            gtk_widget_get_size_request(w, NULL, &hq);
            gtk_widget_set_size_request(w, atoi(val), hq);
        } else if (!strcmp(prop, "height")) {
            int wq;
            gtk_widget_get_size_request(w, &wq, NULL);
            gtk_widget_set_size_request(w, wq, atoi(val));
        }
        free(name); free(prop); free(val);
        return make_num(0);
    }

    if (!strcmp(fn, "get")) {
        if (argc < 3) fatal("line %d: &gtk|get widget prop", line);
        char *name = resolve_arg(args[1]);
        char *prop = resolve_arg(args[2]);
        GtkWidget *w = gtk_find_w(name);
        if (!w) fatal("line %d: widget '%s' not found", line, args[1]);
        Value r = make_str("");
        if (!strcmp(prop, "label")) {
            if (GTK_IS_BUTTON(w)) r = make_str(gtk_button_get_label(GTK_BUTTON(w)));
            else if (GTK_IS_LABEL(w)) r = make_str(gtk_label_get_text(GTK_LABEL(w)));
        } else if (!strcmp(prop, "text")) {
            if (GTK_IS_LABEL(w)) r = make_str(gtk_label_get_text(GTK_LABEL(w)));
        } else if (!strcmp(prop, "title")) {
            if (GTK_IS_WINDOW(w)) r = make_str(gtk_window_get_title(GTK_WINDOW(w)));
        }
        free(name); free(prop);
        return r;
    }

    if (!strcmp(fn, "on")) {
        if (argc < 4) fatal("line %d: &gtk|on widget signal varname", line);
        char *name = resolve_arg(args[1]);
        char *sig = resolve_arg(args[2]);
        char *vname = resolve_arg(args[3]);
        GtkWidget *w = gtk_find_w(name);
        if (!w) fatal("line %d: widget '%s' not found", line, args[1]);
        GtkSigData *sd = safe_alloc(sizeof(GtkSigData));
        strncpy(sd->vname, vname, 63); sd->vname[63] = 0;
        strncpy(sd->sig, sig, 63); sd->sig[63] = 0;
        var_set(vname, make_str(""));
        if (!strcmp(sig, "destroy")) {
            g_signal_connect(w, "destroy", G_CALLBACK(gtk_destroy_cb), NULL);
        } else {
            g_signal_connect(w, sig, G_CALLBACK(gtk_sig_cb), sd);
        }
        free(name); free(sig); free(vname);
        return make_num(0);
    }

    if (!strcmp(fn, "wait")) {
        gtk_window_gone = 0;
        gtk_in_main = 1;
        gtk_main();
        gtk_in_main = 0;
        return make_num(gtk_window_gone ? 1 : 0);
    }

    if (!strcmp(fn, "run")) {
        gtk_main_iteration();
        return make_num(gtk_window_gone ? 1 : 0);
    }

    if (!strcmp(fn, "timeout")) {
        if (argc < 4) fatal("line %d: &gtk|timeout ms signal varname", line);
        int ms = atoi(resolve_arg(args[1]));
        char *sig = resolve_arg(args[2]);
        char *vname = resolve_arg(args[3]);
        GtkSigData *sd = safe_alloc(sizeof(GtkSigData));
        strncpy(sd->vname, vname, 63); sd->vname[63] = 0;
        strncpy(sd->sig, sig, 63); sd->sig[63] = 0;
        var_set(vname, make_str(""));
        g_timeout_add(ms, gtk_timeout_cb, sd);
        free(sig); free(vname);
        return make_num(0);
    }

    if (!strcmp(fn, "quit")) {
        if (gtk_in_main) gtk_main_quit();
        return make_num(0);
    }

    if (!strcmp(fn, "destroy")) {
        if (argc < 2) fatal("line %d: &gtk|destroy name", line);
        char *name = resolve_arg(args[1]);
        GtkWidget *w = gtk_find_w(name);
        if (w) gtk_widget_destroy(w);
        free(name);
        return make_num(0);
    }

    fatal("line %d: unknown gtk function '%s'", line, fn);
    return make_num(0);
}

#endif

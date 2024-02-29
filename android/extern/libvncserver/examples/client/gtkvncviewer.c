
/*
 * Copyright (C) 2007 - Mateus Cesar Groess
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <rfb/rfbclient.h>

static rfbClient *cl;
static gchar *server_cut_text = NULL;
static gboolean framebuffer_allocated = FALSE;
static GtkWidget *window;
static GtkWidget *dialog_connecting = NULL;

/* Redraw the screen from the backing pixmap */
static gboolean expose_event (GtkWidget      *widget,
                              GdkEventExpose *event)
{
	static GdkImage *image = NULL;

	if (framebuffer_allocated == FALSE) {

		rfbClientSetClientData (cl, gtk_init, widget);

		image = gdk_drawable_get_image (widget->window, 0, 0,
		                                widget->allocation.width,
		                                widget->allocation.height);

		cl->frameBuffer= image->mem;

		cl->width  = widget->allocation.width;
		cl->height = widget->allocation.height;

		cl->format.bitsPerPixel = image->bits_per_pixel;
		cl->format.redShift     = image->visual->red_shift;
		cl->format.greenShift   = image->visual->green_shift;
		cl->format.blueShift    = image->visual->blue_shift;

		cl->format.redMax   = (1 << image->visual->red_prec) - 1;
		cl->format.greenMax = (1 << image->visual->green_prec) - 1;
		cl->format.blueMax  = (1 << image->visual->blue_prec) - 1;

		SetFormatAndEncodings (cl);

		framebuffer_allocated = TRUE;

		/* Also disable local cursor */
		GdkCursor* cur = gdk_cursor_new( GDK_BLANK_CURSOR );
		gdk_window_set_cursor (gtk_widget_get_window(GTK_WIDGET(window)), cur);
		gdk_cursor_unref( cur );
	}

	gdk_draw_image (GDK_DRAWABLE (widget->window),
	                widget->style->fg_gc[gtk_widget_get_state(widget)],
	                image,
	                event->area.x, event->area.y,
	                event->area.x, event->area.y,
	                event->area.width, event->area.height);

	return FALSE;
}

struct { int gdk; int rfb; } buttonMapping[] = {
	{ GDK_BUTTON1_MASK, rfbButton1Mask },
	{ GDK_BUTTON2_MASK, rfbButton2Mask },
	{ GDK_BUTTON3_MASK, rfbButton3Mask },
	{ 0, 0 }
};

static gboolean button_event (GtkWidget      *widget,
                              GdkEventButton *event)
{
	int x, y;
	GdkModifierType state;
	int i, buttonMask;

	gdk_window_get_pointer (event->window, &x, &y, &state);

	for (buttonMask = 0, i = 0; buttonMapping[i].gdk; i++)
		if (state & buttonMapping[i].gdk)
			buttonMask |= buttonMapping[i].rfb;
	SendPointerEvent (cl, x, y, buttonMask);

	return TRUE;
}

static gboolean motion_notify_event (GtkWidget *widget,
                                     GdkEventMotion *event)
{
	int x, y;
	GdkModifierType state;
	int i, buttonMask;

	if (event->is_hint)
		gdk_window_get_pointer (event->window, &x, &y, &state);
	else {
		x = event->x;
		y = event->y;
		state = event->state;
	}

	for (buttonMask = 0, i = 0; buttonMapping[i].gdk; i++)
		if (state & buttonMapping[i].gdk)
			buttonMask |= buttonMapping[i].rfb;
	SendPointerEvent (cl, x, y, buttonMask);

	return TRUE;
}

static void got_cut_text (rfbClient *cl, const char *text, int textlen)
{
	if (server_cut_text != NULL) {
		g_free (server_cut_text);
		server_cut_text = NULL;
	}

	server_cut_text = g_strdup (text);
}

void received_text_from_clipboard (GtkClipboard *clipboard,
                                   const gchar *text,
                                   gpointer data)
{
	if (text)
		SendClientCutText (cl, (char *) text, strlen (text));
}

static void clipboard_local_to_remote (GtkMenuItem *menuitem,
                                       gpointer     user_data)
{
	GtkClipboard *clipboard;

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (menuitem),
	                                      GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_request_text (clipboard, received_text_from_clipboard,
	                            NULL);
}

static void clipboard_remote_to_local (GtkMenuItem *menuitem,
                                       gpointer     user_data)
{
	GtkClipboard *clipboard;

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (menuitem),
	                                      GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_set_text (clipboard, server_cut_text,
	                        strlen (server_cut_text));
}

static void request_screen_refresh (GtkMenuItem *menuitem,
                                    gpointer     user_data)
{
	SendFramebufferUpdateRequest (cl, 0, 0, cl->width, cl->height, FALSE);
}

static void send_f8 (GtkMenuItem *menuitem,
                     gpointer     user_data)
{
	SendKeyEvent(cl, XK_F8, TRUE);
	SendKeyEvent(cl, XK_F8, FALSE);
}

static void send_crtl_alt_del (GtkMenuItem *menuitem,
                               gpointer     user_data)
{
	SendKeyEvent(cl, XK_Control_L, TRUE);
	SendKeyEvent(cl, XK_Alt_L, TRUE);
	SendKeyEvent(cl, XK_Delete, TRUE);
	SendKeyEvent(cl, XK_Alt_L, FALSE);
	SendKeyEvent(cl, XK_Control_L, FALSE);
	SendKeyEvent(cl, XK_Delete, FALSE);
}

static void show_connect_window(int argc, char **argv)
{
	GtkWidget *label;
	char buf[256];

	dialog_connecting = gtk_dialog_new_with_buttons ("VNC Viewer",
	                                       NULL,
	                                       GTK_DIALOG_DESTROY_WITH_PARENT,
	                                       /*GTK_STOCK_CANCEL,
	                                       GTK_RESPONSE_CANCEL,*/
	                                       NULL);

	/* FIXME: this works only when address[:port] is at end of arg list */
	char *server;
	if(argc==1)
	    server = "localhost";
	else
	   server = argv[argc-1];
	snprintf(buf, 255, "Connecting to %s...", server);

	label = gtk_label_new (buf);
	gtk_widget_show (label);

	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog_connecting)->vbox),
	                   label);

	gtk_widget_show (dialog_connecting);

	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void show_popup_menu()
{
	GtkWidget *popup_menu;
	GtkWidget *menu_item;

	popup_menu = gtk_menu_new ();

	menu_item = gtk_menu_item_new_with_label ("Dismiss popup");
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menu_item);

	menu_item = gtk_menu_item_new_with_label ("Clipboard: local -> remote");
	g_signal_connect (G_OBJECT (menu_item), "activate",
	                  G_CALLBACK (clipboard_local_to_remote), NULL);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menu_item);

	menu_item = gtk_menu_item_new_with_label ("Clipboard: local <- remote");
	g_signal_connect (G_OBJECT (menu_item), "activate",
	                  G_CALLBACK (clipboard_remote_to_local), NULL);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menu_item);

	menu_item = gtk_menu_item_new_with_label ("Request refresh");
	g_signal_connect (G_OBJECT (menu_item), "activate",
	                  G_CALLBACK (request_screen_refresh), NULL);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menu_item);

	menu_item = gtk_menu_item_new_with_label ("Send ctrl-alt-del");
	g_signal_connect (G_OBJECT (menu_item), "activate",
	                  G_CALLBACK (send_crtl_alt_del), NULL);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menu_item);

	menu_item = gtk_menu_item_new_with_label ("Send F8");
	g_signal_connect (G_OBJECT (menu_item), "activate",
	                  G_CALLBACK (send_f8), NULL);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menu_item);

	gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL, NULL, NULL, 0,
	                gtk_get_current_event_time());
}

static rfbKeySym gdkKey2rfbKeySym(guint keyval)
{
	rfbKeySym k = 0;
	switch(keyval) {
	case GDK_BackSpace: k = XK_BackSpace; break;
	case GDK_Tab: k = XK_Tab; break;
	case GDK_Clear: k = XK_Clear; break;
	case GDK_Return: k = XK_Return; break;
	case GDK_Pause: k = XK_Pause; break;
	case GDK_Escape: k = XK_Escape; break;
	case GDK_space: k = XK_space; break;
	case GDK_Delete: k = XK_Delete; break;
	case GDK_KP_0: k = XK_KP_0; break;
	case GDK_KP_1: k = XK_KP_1; break;
	case GDK_KP_2: k = XK_KP_2; break;
	case GDK_KP_3: k = XK_KP_3; break;
	case GDK_KP_4: k = XK_KP_4; break;
	case GDK_KP_5: k = XK_KP_5; break;
	case GDK_KP_6: k = XK_KP_6; break;
	case GDK_KP_7: k = XK_KP_7; break;
	case GDK_KP_8: k = XK_KP_8; break;
	case GDK_KP_9: k = XK_KP_9; break;
	case GDK_KP_Decimal: k = XK_KP_Decimal; break;
	case GDK_KP_Divide: k = XK_KP_Divide; break;
	case GDK_KP_Multiply: k = XK_KP_Multiply; break;
	case GDK_KP_Subtract: k = XK_KP_Subtract; break;
	case GDK_KP_Add: k = XK_KP_Add; break;
	case GDK_KP_Enter: k = XK_KP_Enter; break;
	case GDK_KP_Equal: k = XK_KP_Equal; break;
	case GDK_Up: k = XK_Up; break;
	case GDK_Down: k = XK_Down; break;
	case GDK_Right: k = XK_Right; break;
	case GDK_Left: k = XK_Left; break;
	case GDK_Insert: k = XK_Insert; break;
	case GDK_Home: k = XK_Home; break;
	case GDK_End: k = XK_End; break;
	case GDK_Page_Up: k = XK_Page_Up; break;
	case GDK_Page_Down: k = XK_Page_Down; break;
	case GDK_F1: k = XK_F1; break;
	case GDK_F2: k = XK_F2; break;
	case GDK_F3: k = XK_F3; break;
	case GDK_F4: k = XK_F4; break;
	case GDK_F5: k = XK_F5; break;
	case GDK_F6: k = XK_F6; break;
	case GDK_F7: k = XK_F7; break;
	case GDK_F8: k = XK_F8; break;
	case GDK_F9: k = XK_F9; break;
	case GDK_F10: k = XK_F10; break;
	case GDK_F11: k = XK_F11; break;
	case GDK_F12: k = XK_F12; break;
	case GDK_F13: k = XK_F13; break;
	case GDK_F14: k = XK_F14; break;
	case GDK_F15: k = XK_F15; break;
	case GDK_Num_Lock: k = XK_Num_Lock; break;
	case GDK_Caps_Lock: k = XK_Caps_Lock; break;
	case GDK_Scroll_Lock: k = XK_Scroll_Lock; break;
	case GDK_Shift_R: k = XK_Shift_R; break;
	case GDK_Shift_L: k = XK_Shift_L; break;
	case GDK_Control_R: k = XK_Control_R; break;
	case GDK_Control_L: k = XK_Control_L; break;
	case GDK_Alt_R: k = XK_Alt_R; break;
	case GDK_Alt_L: k = XK_Alt_L; break;
	case GDK_Meta_R: k = XK_Meta_R; break;
	case GDK_Meta_L: k = XK_Meta_L; break;
#if 0
	/* TODO: find out keysyms */
	case GDK_Super_L: k = XK_LSuper; break;      /* left "windows" key */
	case GDK_Super_R: k = XK_RSuper; break;      /* right "windows" key */
	case GDK_Multi_key: k = XK_Compose; break;
#endif
	case GDK_Mode_switch: k = XK_Mode_switch; break;
	case GDK_Help: k = XK_Help; break;
	case GDK_Print: k = XK_Print; break;
	case GDK_Sys_Req: k = XK_Sys_Req; break;
	case GDK_Break: k = XK_Break; break;
	default: break;
	}
	if (k == 0) {
		if (keyval < 0x100)
			k = keyval;
		else
			rfbClientLog ("Unknown keysym: %d\n", keyval);
	}

	return k;
}

static gboolean key_event (GtkWidget *widget, GdkEventKey *event,
                                 gpointer user_data)
{
	if ((event->type == GDK_KEY_PRESS) && (event->keyval == GDK_F8))
		show_popup_menu();
	else
		SendKeyEvent(cl, gdkKey2rfbKeySym (event->keyval),
		             (event->type == GDK_KEY_PRESS) ? TRUE : FALSE);
	return FALSE;
}

void quit ()
{
	exit (0);
}

static rfbBool resize (rfbClient *client) {
	GtkWidget *scrolled_window;
	GtkWidget *drawing_area=NULL;
	static char first=TRUE;
	int tmp_width, tmp_height;

	if (first) {
		first=FALSE;

		/* Create the drawing area */

		drawing_area = gtk_drawing_area_new ();
		gtk_widget_set_size_request (GTK_WIDGET (drawing_area),
		                             client->width, client->height);

		/* Signals used to handle backing pixmap */

		g_signal_connect (G_OBJECT (drawing_area), "expose_event",
		                  G_CALLBACK (expose_event), NULL);

		/* Event signals */

		g_signal_connect (G_OBJECT (drawing_area),
		                  "motion-notify-event",
		                  G_CALLBACK (motion_notify_event), NULL);
		g_signal_connect (G_OBJECT (drawing_area),
		                  "button-press-event",
		                  G_CALLBACK (button_event), NULL);
		g_signal_connect (G_OBJECT (drawing_area),
		                  "button-release-event",
		                  G_CALLBACK (button_event), NULL);

		gtk_widget_set_events (drawing_area, GDK_EXPOSURE_MASK
		                       | GDK_LEAVE_NOTIFY_MASK
		                       | GDK_BUTTON_PRESS_MASK
		                       | GDK_BUTTON_RELEASE_MASK
		                       | GDK_POINTER_MOTION_MASK
		                       | GDK_POINTER_MOTION_HINT_MASK);

		gtk_widget_show (drawing_area);

		scrolled_window = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
		                                GTK_POLICY_AUTOMATIC,
		                                GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_add_with_viewport (
		                  GTK_SCROLLED_WINDOW (scrolled_window),
		                  drawing_area);
		g_signal_connect (G_OBJECT (scrolled_window),
		                  "key-press-event", G_CALLBACK (key_event),
		                  NULL);
		g_signal_connect (G_OBJECT (scrolled_window),
		                  "key-release-event", G_CALLBACK (key_event),
		                  NULL);
		gtk_widget_show (scrolled_window);

		window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (window), client->desktopName);
		gtk_container_add (GTK_CONTAINER (window), scrolled_window);
		tmp_width = (int) (
		            gdk_screen_get_width (gdk_screen_get_default ())
		            * 0.85);
		if (client->width > tmp_width) {
			tmp_height = (int) (
			             gdk_screen_get_height (
			                     gdk_screen_get_default ())
			             * 0.85);
			gtk_widget_set_size_request (window,
			                             tmp_width, tmp_height);
		} else {
			gtk_widget_set_size_request (window,
			       client->width + 2,
			       client->height + 2);
		}

		g_signal_connect (G_OBJECT (window), "destroy",
		                  G_CALLBACK (quit), NULL);

		gtk_widget_show (window);
	} else {
		gtk_widget_set_size_request (GTK_WIDGET (drawing_area),
		                             client->width, client->height);
	}

	return TRUE;
}

static void update (rfbClient *cl, int x, int y, int w, int h) {
	if (dialog_connecting != NULL) {
		gtk_widget_destroy (dialog_connecting);
		dialog_connecting = NULL;
	}

	GtkWidget *drawing_area = rfbClientGetClientData (cl, gtk_init);

	if (drawing_area != NULL)
		gtk_widget_queue_draw_area (drawing_area, x, y, w, h);
}

static void kbd_leds (rfbClient *cl, int value, int pad) {
        /* note: pad is for future expansion 0=unused */
        fprintf (stderr, "Led State= 0x%02X\n", value);
        fflush (stderr);
}

/* trivial support for textchat */
static void text_chat (rfbClient *cl, int value, char *text) {
        switch (value) {
        case rfbTextChatOpen:
                fprintf (stderr, "TextChat: We should open a textchat window!\n");
                TextChatOpen (cl);
                break;
        case rfbTextChatClose:
                fprintf (stderr, "TextChat: We should close our window!\n");
                break;
        case rfbTextChatFinished:
                fprintf (stderr, "TextChat: We should close our window!\n");
                break;
        default:
                fprintf (stderr, "TextChat: Received \"%s\"\n", text);
                break;
        }
        fflush (stderr);
}

static gboolean on_entry_key_press_event (GtkWidget *widget, GdkEventKey *event,
                                          gpointer user_data)
{
	if (event->keyval == GDK_Escape)
		gtk_dialog_response (GTK_DIALOG(user_data), GTK_RESPONSE_REJECT);
	else if (event->keyval == GDK_Return)
		gtk_dialog_response (GTK_DIALOG(user_data), GTK_RESPONSE_ACCEPT);

	return FALSE;
}

static void GtkErrorLog (const char *format, ...)
{
	GtkWidget *dialog, *label;
	va_list args;
	char buf[256];

	if (dialog_connecting != NULL) {
		gtk_widget_destroy (dialog_connecting);
		dialog_connecting = NULL;
	}

	va_start (args, format);
	vsnprintf (buf, 255, format, args);
	va_end (args);

	if (g_utf8_validate (buf, strlen (buf), NULL)) {
		label = gtk_label_new (buf);
	} else {
		const gchar *charset;
		gchar       *utf8;

		(void) g_get_charset (&charset);
		utf8 = g_convert_with_fallback (buf, strlen (buf), "UTF-8",
		                                charset, NULL, NULL, NULL, NULL);

		if (utf8) {
			label = gtk_label_new (utf8);
			g_free (utf8);
		} else {
			label = gtk_label_new (buf);
			g_warning ("Message Output is not in UTF-8"
			           "nor in locale charset.\n");
		}
	}

	dialog = gtk_dialog_new_with_buttons ("Error",
	                                       NULL,
	                                       GTK_DIALOG_DESTROY_WITH_PARENT,
	                                       GTK_STOCK_OK,
	                                       GTK_RESPONSE_ACCEPT,
	                                       NULL);
	label = gtk_label_new (buf);
	gtk_widget_show (label);

	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
	                   label);
	gtk_widget_show (dialog);

	switch (gtk_dialog_run (GTK_DIALOG (dialog))) {
	case GTK_RESPONSE_ACCEPT:
		break;
	default:
		break;
	}
	gtk_widget_destroy (dialog);
}

static void GtkDefaultLog (const char *format, ...)
{
	va_list args;
	char buf[256];
	time_t log_clock;

	va_start (args, format);

	time (&log_clock);
	strftime (buf, 255, "%d/%m/%Y %X ", localtime (&log_clock));
	fprintf (stdout, "%s", buf);

	vfprintf (stdout, format, args);
	fflush (stdout);

	va_end (args);
}

static char * get_password (rfbClient *client)
{
	GtkWidget *dialog, *entry;
	char *password;

	gtk_widget_destroy (dialog_connecting);
	dialog_connecting = NULL;

	dialog = gtk_dialog_new_with_buttons ("Password",
	                                       NULL,
	                                       GTK_DIALOG_DESTROY_WITH_PARENT,
	                                       GTK_STOCK_CANCEL,
	                                       GTK_RESPONSE_REJECT,
	                                       GTK_STOCK_OK,
	                                       GTK_RESPONSE_ACCEPT,
	                                       NULL);
	entry = gtk_entry_new ();
	gtk_entry_set_visibility (GTK_ENTRY (entry),
	                          FALSE);
	g_signal_connect (GTK_OBJECT(entry), "key-press-event",
	                    G_CALLBACK(on_entry_key_press_event),
	                    GTK_OBJECT (dialog));
	gtk_widget_show (entry);

	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
	                   entry);
	gtk_widget_show (dialog);

	switch (gtk_dialog_run (GTK_DIALOG (dialog))) {
	case GTK_RESPONSE_ACCEPT:
		password = strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
		break;
	default:
		password = NULL;
		break;
	}
	gtk_widget_destroy (dialog);
	return password;
}

int main (int argc, char *argv[])
{
	int i;
	GdkImage *image;

	rfbClientLog = GtkDefaultLog;
	rfbClientErr = GtkErrorLog;

	gtk_init (&argc, &argv);

	/* create a dummy image just to make use of its properties */
	image = gdk_image_new (GDK_IMAGE_FASTEST, gdk_visual_get_system(),
				200, 100);

	cl = rfbGetClient (image->depth / 3, 3, image->bpp);

	cl->format.redShift     = image->visual->red_shift;
	cl->format.greenShift   = image->visual->green_shift;
	cl->format.blueShift    = image->visual->blue_shift;

	cl->format.redMax   = (1 << image->visual->red_prec) - 1;
	cl->format.greenMax = (1 << image->visual->green_prec) - 1;
	cl->format.blueMax  = (1 << image->visual->blue_prec) - 1;

	g_object_unref (image);

	cl->MallocFrameBuffer = resize;
	cl->canHandleNewFBSize = TRUE;
	cl->GotFrameBufferUpdate = update;
	cl->GotXCutText = got_cut_text;
	cl->HandleKeyboardLedState = kbd_leds;
	cl->HandleTextChat = text_chat;
	cl->GetPassword = get_password;

	show_connect_window (argc, argv);

	if (!rfbInitClient (cl, &argc, argv))
		return 1;

	while (1) {
		while (gtk_events_pending ())
			gtk_main_iteration ();
		i = WaitForMessage (cl, 500);
		if (i < 0)
			return 0;
		if (i && framebuffer_allocated == TRUE)
			if (!HandleRFBServerMessage(cl))
				return 0;
	}

	gtk_main ();

	return 0;
}


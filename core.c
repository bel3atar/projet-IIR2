#include "core.h"
static void log_event(GtkListStore *const logstore, const guint clk, gchar *const processus, gchar *const event) {
	GtkTreeIter i;
	gtk_list_store_append(logstore, &i);
	gtk_list_store_set(logstore, &i, COL_LOG_TIME, clk, COL_LOG_NAME, processus, COL_LOG_EVENT, event, -1);
}
static void log_stats(GtkListStore *const statstore, gchar *const nom, const guint fin, const guint sejour, const guint attente) {
	GtkTreeIter i;
	gtk_list_store_append(statstore, &i);
	gtk_list_store_set(statstore, &i, COL_NOM, nom, COL_FIN, fin, COL_SEJOUR, sejour, COL_ATTENTE, attente, -1);
}
static void incrementer_attente(GQueue *const f) {
	GList *p;
	if (g_queue_is_empty(f) == FALSE)
		for (p = f->head->next; p != NULL; p = g_list_next(p))
			((JobData*)p->data)->tempsAttente ++;
}
static void accueillir(Data * const d, const guint clk) {
	GSList *j;
	for (j = d->liste_attente; j != NULL; j = g_slist_next(j))
		if (clk >= ((JobData*)(j->data))->dateArrivee) {
			log_event(d->logstore, ((JobData*)(j->data))->dateArrivee, ((JobData*)(j->data))->nom, "Arrivée");
			d->liste_attente = g_slist_next(d->liste_attente);
			g_queue_push_tail(d->file, j->data);
		} else
			break;
}
static gboolean chaine_vide(gchar *const s) {
	gsize i;
	if (s != NULL) {
		if (s[0] == '\0')
			return TRUE;
		for (i = strlen(s) - 1; i != 0; i--)
			if (g_ascii_isspace(s[i]) == FALSE)
				break;
		if (i == 0)
			if (g_ascii_isspace(s[0]))
				return TRUE;
		return FALSE;
	} else
		return TRUE;
}
static void lire_fichier(gchar *const filename, GtkTreeModel *const mainstore) {
	gchar *file, **lines, **i, **line;
	GtkTreeIter iter;
	guint exec;
	if (g_file_get_contents(filename, &file, NULL, NULL) == FALSE)
		g_error("Impossible de lire le fichier %s\n", filename);
	else {
		gtk_tree_model_get_iter_first(GTK_TREE_MODEL(mainstore), &iter);
		lines = g_strsplit(file, "\n", 0);
		g_free(file);
		for (i = lines; *i != NULL; i++) {
			line = g_strsplit(*i, ",", 3);
			if (g_strv_length(line) != 3)
				continue;
			if(chaine_vide(line[0]))
				continue;
			exec = (guint)strtoul(line[2], NULL, 10);
			if(exec == 0)
				continue;
			gtk_list_store_append(GTK_LIST_STORE(mainstore), &iter);
			gtk_list_store_set(	GTK_LIST_STORE(mainstore), &iter,
								COL_NOM, g_strstrip(line[0]),
								COL_TEMPS_ARRIVEE, (guint)strtoul(line[1], NULL, 10),
								COL_TEMPS_EXECUTION, exec, -1);
		}
		g_strfreev(lines);
	}
}
static gint compare_function(JobData *const A, JobData *const B) {
	guint a = A->dateArrivee, b = B->dateArrivee;
	return a < b ? -1 : (a > b ? 1 : 0);
}
static void free_func(JobData *p, gpointer user_data) {
	g_free(p->nom);
	g_free(p);
}
static void list_to_lists(Data *const d, guint *const nombre_porcessus, glong *const tme) {
	JobData *data;
	GtkTreeIter i;
	GtkTreeModel *model = gtk_tree_view_get_model(d->mainlist);
	*nombre_porcessus = 0;
	d->liste_attente = NULL;
	gtk_tree_model_get_iter_first(model, &i);
	do {
		data = g_malloc(sizeof(JobData));
		data->tempsAttente = 0;
		gtk_tree_model_get(model, &i, COL_NOM, &data->nom, COL_TEMPS_ARRIVEE, &data->dateArrivee, COL_TEMPS_EXECUTION, &data->tempsRestant, -1);
		*tme -= data->dateArrivee;
		if (data->dateArrivee == 0)
			g_queue_push_tail(d->file, data);
		else
			d->liste_attente = g_slist_insert_sorted(d->liste_attente, data, (GCompareFunc)compare_function);
		++*nombre_porcessus;
	} while (gtk_tree_model_iter_next(model, &i));
}
static void start_simulation(Data *d) {
	guint quantum = gtk_adjustment_get_value(d->quantum), delta = gtk_adjustment_get_value(d->delta);
	guint i, changements = 0, clk = 0, nombre_porcessus = 0;
	GString *message = g_string_new("");
	GtkMessageDialog *msgbox;
	glong tme = 0;
	d->sim_q = quantum;
	d->sim_d = delta;
	d->file = g_queue_new();
	list_to_lists(d, &nombre_porcessus, &tme);
	for (;d->liste_attente != NULL || g_queue_is_empty(d->file) == FALSE; ) {
		if (g_queue_is_empty(d->file) == FALSE) { /* file 3amra */
			if (((JobData*)d->file->head->data)->tempsRestant <= quantum) { /* lwa9t li b9a sghar men lquantum */
				i = ((JobData*)d->file->head->data)->tempsRestant;
				clk += i;
				tme += clk;
				do {
					incrementer_attente(d->file);
					accueillir(d, clk);
				} while (--i);
				log_event(d->logstore, clk, ((JobData*)(d->file->head->data))->nom, "Fin");
				log_stats(d->statstore, ((JobData*)d->file->head->data)->nom, clk, clk - ((JobData*)d->file->head->data)->dateArrivee, 			  ((JobData*)d->file->head->data)->tempsAttente);
				g_free(((JobData*)d->file->head->data)->nom);
				g_free((JobData*)g_queue_pop_head(d->file));
			} else { /* lwa9t li b9a kbar men lquantum */
				clk += quantum;
				i = quantum;
				do {
					incrementer_attente(d->file);
					accueillir(d, clk);
				} while (--i);
				((JobData*)d->file->head->data)->tempsRestant -= quantum;
				if (g_queue_get_length(d->file) != 1) { /* ila makanch ghir 1 fla file*/
					changements++;
					log_event(d->logstore, clk, ((JobData*)d->file->head->data)->nom, "Fin du quantum et remise en queue.");
					for (i = delta; i != 0; i--) {
						incrementer_attente(d->file);
						accueillir(d, clk);
					}
					g_queue_push_tail(d->file, g_queue_pop_head(d->file));
					clk += delta;
				}
				accueillir(d, clk);
			}
		} else { /*file khawya*/
			clk = ((JobData*)(d->liste_attente->data))->dateArrivee;
			accueillir(d, clk);
		}
	}
	g_queue_foreach(d->file, (GFunc)free_func, NULL);
	g_queue_free(d->file);
	g_slist_free(d->liste_attente);
	g_string_printf(message, "Simulation terminée:\n\tTemps moyen d'exécution: %g.\n\tNombre de changements de contexte: %u", (float)tme / nombre_porcessus, changements);
	log_event(d->logstore, clk, "\tF\nF\tI\tN\n\tN", message->str);
	msgbox = GTK_MESSAGE_DIALOG(gtk_message_dialog_new (GTK_WINDOW(d->window), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Simulation terminée:\n\tDate de fin de tous les processus: %u\n\tTemps moyen d'exécution: %g\n\tNombre de changements de contexte: %u", clk, (float)tme / nombre_porcessus, changements));
	gtk_dialog_run(GTK_DIALOG (msgbox));
	gtk_widget_destroy(GTK_WIDGET(msgbox));
	g_string_free(message, TRUE);
}
void apropos(GtkWidget *w, gpointer data) {
	gtk_dialog_run(GTK_DIALOG(data));
	gtk_widget_hide(GTK_WIDGET(data)); /*holako*/
}
void open_clicked(GtkWidget *w, Data *data) {
	GtkWidget *f = gtk_file_chooser_dialog_new("Choisissez un fichier à ouvrir", GTK_WINDOW(data->window), GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	if (gtk_dialog_run(GTK_DIALOG(f)) == GTK_RESPONSE_ACCEPT)
		lire_fichier(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f)), gtk_tree_view_get_model((GtkTreeView*) data->mainlist));
	gtk_widget_destroy(f);
}
void play_clicked(GtkWidget *w, Data *data) {
	GtkTreeIter iter;
	if (gtk_tree_model_get_iter_first(gtk_tree_view_get_model(data->mainlist), &iter)) {
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(data->statstore), &iter))
			while(gtk_list_store_remove(data->statstore, &iter));
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(data->logstore), &iter))
			while(gtk_list_store_remove(data->logstore, &iter));
		start_simulation(data);
	}
}
void clear_list(GtkWidget *w, GtkTreeModel *data) {
	GtkTreeIter iter;
	if (gtk_tree_model_get_iter_first(data, &iter))
		while(gtk_list_store_remove(GTK_LIST_STORE(data), &iter));
}
void remove_item(GtkWidget *widget, GtkTreeView *treeview) {
	GtkTreeIter iter;
	if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(treeview), NULL, &iter))
		gtk_list_store_remove(GTK_LIST_STORE(gtk_tree_view_get_model(treeview)), &iter);
}
void add_item(GtkWidget *button, GtkTreeModel *model) {
	GtkTreeIter iter;
	static unsigned i = 1;
	GString *nom = g_string_new("Processus ");
	g_string_append_printf(nom, "%u", i++);
	gtk_list_store_append(GTK_LIST_STORE(model), &iter);
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, COL_NOM, nom->str, COL_TEMPS_ARRIVEE, rand() % 20, COL_TEMPS_EXECUTION, rand() % 60 + 1,-1);
	g_string_free(nom, TRUE);
}
void cell_edited(GtkCellRendererText *cell, const gchar *path_string, const gchar *new_text, GtkTreeModel *model) {
	const guint col = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(cell), "colnum"));
	GtkTreePath *path = gtk_tree_path_new_from_string(path_string);
	GtkTreeIter iter;
	gchar *old_text;
	guint val;
	gtk_tree_model_get_iter(model, &iter, path);
	switch (col) {
		case COL_NOM:
			gtk_tree_model_get(model, &iter, COL_NOM, &old_text, -1);
			g_free(old_text);
			gtk_list_store_set(GTK_LIST_STORE(model), &iter, COL_NOM, g_strdup(new_text), -1);
			break;
		case COL_TEMPS_ARRIVEE:
			val = (guint)strtoul(new_text, NULL, 10);
			gtk_list_store_set (GTK_LIST_STORE(model), &iter, col, val, -1);
			break;
		case COL_TEMPS_EXECUTION:
			if ((val = (guint)strtoul(new_text, NULL, 10)))
				gtk_list_store_set (GTK_LIST_STORE(model), &iter, col, val, -1);
			break;
	}
	gtk_tree_path_free(path);
}
void save_clicked(GtkWidget *w, Data *d) {
	GtkTreeModel *mainn = gtk_tree_view_get_model(d->mainlist), *stat = GTK_TREE_MODEL(d->statstore);
	GtkWidget *dialog;
	GtkTreeIter i;
	GString *s;
	gchar *nom_;
	gchar *nom;
	guint v;
	if (gtk_tree_model_get_iter_first(mainn, &i) && gtk_tree_model_get_iter_first(stat, &i)) {
		dialog = gtk_file_chooser_dialog_new("Enregistrer...", GTK_WINDOW(d->window), GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
		gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
			s = g_string_new  (	"<!DOCTYPE html>\n"
								"<html>"
								"<head>"
									"<meta charset=UTF-8 />"
									"<title>Simulation</title>"
								"</head>"
								"<body>"
									"<!--\n");
			gtk_tree_model_get_iter_first(mainn, &i);
			do {
				gtk_tree_model_get(mainn, &i, COL_NOM, &nom, COL_TEMPS_ARRIVEE, &v, -1);
				g_string_append_printf(s, "%s, %u, ", nom, v);
				g_free(nom);
				gtk_tree_model_get(mainn, &i, COL_TEMPS_EXECUTION, &v, -1);
				g_string_append_printf(s, "%u\n<br />\n", v);
			} while(gtk_tree_model_iter_next(mainn, &i));
			g_string_append(s,	"-->"
								"<h1>Processus simulés</h1>"
								"<table border=\"1\">"
									"<tr>"
										"<th>Nom du processus</th>"
										"<th>Date d'arrivée</th>"
										"<th>Temps d'exécution</th>"
									"</tr>");

			gtk_tree_model_get_iter_first(mainn, &i);
			do {
				g_string_append(s,"<tr>");
				gtk_tree_model_get(mainn, &i, COL_NOM, &nom, COL_TEMPS_ARRIVEE, &v, -1);
				g_string_append_printf(s, "<td>%s</td><td>%u</td>", nom, v);
				g_free(nom);
				gtk_tree_model_get(mainn, &i, COL_TEMPS_EXECUTION, &v, -1);
				g_string_append_printf(s, "<td>%u</td>", v);
				g_string_append(s,"</tr>");
			} while (gtk_tree_model_iter_next(mainn, &i));

			
			g_string_append(s,	"</table><h1>Statistiques de la simulation</h1>"
								"<dl>"
									"<dt>Quantum</dt>"
									"<dd>");
			g_string_append_printf(s, "%u", d->sim_q);
			g_string_append(s,		"</dd>"
									"<dt>Temps de changement de contexte</dt>"
									"<dd>");
			g_string_append_printf(s, "%u", d->sim_d);
			g_string_append(s,		"</dd>"
								"</dl>"
								"<table border=\"1\">"
									"<tr>"
										"<th>Nom du processus</th>"
										"<th>Date de fin</th>"
										"<th>Temps de séjour</th>"
										"<th>Temps d'attente</th>"
									"</tr>");
			gtk_tree_model_get_iter_first(stat, &i);
			do {
				g_string_append(s, "<tr>");
				gtk_tree_model_get(stat, &i, COL_NOM, &nom, COL_FIN, &v, -1);
				g_string_append_printf(s, "<td>%s</td><td>%u</td>", nom, v);
				g_free(nom);
				gtk_tree_model_get(stat, &i, COL_SEJOUR, &v, -1);
				g_string_append_printf(s, "<td>%u</td>", v);
				gtk_tree_model_get(stat, &i, COL_ATTENTE, &v, -1);
				g_string_append_printf(s, "<td>%u</td>", v);
				g_string_append(s, "</tr>");
			} while(gtk_tree_model_iter_next(stat, &i));
			g_string_append(s, "</table></body></html>");
			nom = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
			if (g_str_has_suffix(nom, ".html") == FALSE){
				nom_ = g_strconcat(nom, ".html", NULL);
				g_free(nom);
				nom = nom_;
			}
			g_file_set_contents(nom, s->str, -1, NULL);
			g_string_free(s, TRUE);
			g_free(nom);
		}
		gtk_widget_destroy(dialog);
	}
}
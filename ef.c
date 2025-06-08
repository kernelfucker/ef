/* See LICENSE for license details */
/* ef - powerful text editor */

#include "ef.h"

const int so = STDOUT_FILENO;
const int si = STDIN_FILENO;

typedef struct{
	char *text;
	size_t length;
	
} efline;

struct efstate{
	struct termios tios;
	efline *lines;
	int num_lines;
	int cursor_x;
	int cursor_y;
	int screen_rows;
	int screen_cols;
	char status[128];
	char line[128];
	int lost;
	char *file;
} end;

void e(const char *s){
	write(so, clear_screen, 4);
	write(so, cursor_home, 3);
	write(so, cursor_show, 6);
	perror(s);
	exit(1);
}

void disable_rowm(){
	if(tcsetattr(si, TCSAFLUSH, &end.tios) == -1)
		e("tcsetattr");
	write(so, block_cursor, strlen(block_cursor));
	write(so, cursor_show, 6);
}

void enable_rowm(){
	if(tcgetattr(si, &end.tios) == -1) e("tcgetattr");
	atexit(disable_rowm);
	struct termios raw = end.tios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	if(tcsetattr(si, TCSAFLUSH, &raw) == -1) e("tcsetattr");
	write(so, block_cursor, strlen(block_cursor));
}

void get_ws(){
	struct winsize ws;
	if(ioctl(so, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
		ws.ws_row = 24;
		ws.ws_col = 80;
	}

	end.screen_rows = ws.ws_row;
	end.screen_cols = ws.ws_col;
}

void ps(){
	snprintf(end.line, sizeof(end.line),
		"control-q: quit | control-s: save | pos: %d,%d",
		end.cursor_y + 1, end.cursor_x + 1);
}

void ef(char *file){
	get_ws();
	end.cursor_x = 0;
	end.cursor_y = 0;
	end.lost = 0;
	end.num_lines = 1;
	end.file = file ? strdup(file) : strdup("out");
	end.lines = malloc(sizeof(efline) * max_lines);
	end.lines[0].text = malloc(1);
	end.lines[0].text[0] = '\0';
	end.lines[0].length = 0;
	FILE *fp = fopen(end.file, "r");
	if(fp){
		char *line = NULL;
		size_t len = 0;
		ssize_t read;
		int line_num = 0;
		while((read = getline(&line, &len, fp)) != -1){
			if(read > 0 && line[read-1] == '\n'){
				line[read-1] = '\0';
				read--;
			}

			if(line_num >= max_lines) break;
			end.lines[line_num].text = malloc(read + 1);
			memcpy(end.lines[line_num].text, line, read);
			end.lines[line_num].text[read] = '\0';
			end.lines[line_num].length = read;
			end.lines[line_num].length = read;
			line_num++;
		}

		free(line);
		fclose(fp);
		end.num_lines = line_num;
		if(end.num_lines == 0){ 
			end.num_lines = 1;
			end.lines[0].text = malloc(1);
			end.lines[0].text[0] = '\0';
			end.lines[0].length = 0;
		}
	}

	snprintf(end.status, sizeof(end.status), "ef: %s | %s | %dx%d | mod: %d",
		ef_version, end.file, end.screen_cols, end.screen_rows, end.lost);
	ps();
}

void in_char(char c){
	if(end.cursor_y >= end.num_lines) return;
	efline *line = &end.lines[end.cursor_y];

	char *newt = realloc(line->text, line->length + 2);
	if(!newt) return;
	line->text = newt;
	if(end.cursor_x < line->length){
		memmove(line->text + end.cursor_x + 1, line->text + end.cursor_x, line->length - end.cursor_x);
	}

	line->text[end.cursor_x] = c;
	line->length++;
	line->text[line->length] = '\0';
	end.cursor_x++;
	ps();
	end.lost = 1;
}

void in_newl(){
	if(end.num_lines >= max_lines) return;
	for(int i = end.num_lines; i > end.cursor_y + 1; i--){
		end.lines[i] = end.lines[i-1];
	}

	efline *cl = &end.lines[end.cursor_y];
	int spos = end.cursor_x;
	int tlen = cl->length - spos;
	efline *nl = &end.lines[end.cursor_y+1];
	if(tlen > 0){
		nl->text = malloc(tlen + 1);
		memcpy(nl->text, cl->text + spos, tlen);
		nl->text[tlen] = '\0';
		nl->length = tlen;
	} else {
		nl->text = malloc(1);
		nl->text[0] = '\0';
		nl->length = 0;
	}

	char *newct = realloc(cl->text, spos + 1);
	if(newct){
		cl->text = newct;
		cl->text[spos] = '\0';
		cl->length = spos;
	}

	end.num_lines++;
	end.cursor_y++;
	end.cursor_x = 0;
	ps();
	end.lost = 1;
}

void savefl(){
	FILE *fp = fopen(end.file, "w");
	if(!fp){
		snprintf(end.line, sizeof(end.line), "error saving to %s", end.file);
		return;
	}

	for(int i = 0; i < end.num_lines; i++){
		fwrite(end.lines[i].text, 1, end.lines[i].length, fp);
		if(i < end.num_lines - 1){
			fputc('\n', fp);
		}
	}

	fclose(fp);
	end.lost = 0;
	snprintf(end.line, sizeof(end.line), "saved %d lines to %s", end.num_lines, end.file);
}

void dscrn(){
	write(so, clear_screen, 4);
	write(so, cursor_home, 3);
	int ch = end.screen_rows - 2;
	int lt = (end.num_lines < ch) ? end.num_lines : ch;
	for(int l = 0; l < lt; l++){
		write(so, end.lines[l].text, end.lines[l].length);
		write(so, erase_line, 3);
		write(so, "\r\n", 2);
	}

	for(int l = lt; l < ch; l++){
		write(so, erase_line, 3);
		write(so, "\r\n", 2);
	}

	snprintf(end.status, sizeof(end.status), "ef: %s | %s | %dx%d | mod: %d",
		ef_version, end.file, end.screen_cols, end.screen_rows, end.lost);
	write(so, end.status, strlen(end.status));
	int st = end.screen_cols - strlen(end.status);
	if(st > 0){
		for(int i = 0; i < st; i++) write(so, " ", 1);
	}

	write(so, text_reset, 4);
	write(so, "\r\n", 2);
	write(so, end.line, strlen(end.line));
	int lp = end.screen_cols - strlen(end.line);
	if(lp > 0){
		for(int i = 0; i < lp; i++) write(so, " ", 1);
	}

	write(so, text_reset, 4);
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (end.cursor_y < ch ? end.cursor_y : ch - 1) + 1, end.cursor_x + 1);
	write(so, buf, strlen(buf));
	write(so, block_cursor, strlen(block_cursor));
}

void pk(){
	char p;
	if(read(si, &p, 1) == 1){
		switch(p){
			case qkey:
				if(end.lost){
					snprintf(end.line, sizeof(end.line), "unsaved changes, press control-q again to quit");
					end.lost = 0;
				} else {
					write(so, clear_screen, 4);
					write(so, cursor_home, 3);
					exit(0);
				}

				break;
			case skey:
				savefl();
				break;
			case '\n':
			case '\r':
				in_newl();
				break;
			case '\t':
				for(int i = 0; i < tab_stop; i++){
					in_char(' ');
				}

				break;
			case 127:
			case 8:
				if(end.cursor_x > 0){
					efline *ln = &end.lines[end.cursor_y];
					if(end.cursor_x <= ln->length){
						memmove(ln->text + end.cursor_x - 1,
							ln->text + end.cursor_x,
							ln->length - end.cursor_x);
						ln->length--;
						char *t = realloc(ln->text, ln->length + 1);
						if(t) ln->text = t;
						ln->text[ln->length] = '\0';
						end.cursor_x--;
						ps();
						end.lost = 1;
					}
				} else if(end.cursor_y > 0){
					efline *prev = &end.lines[end.cursor_y-1];
					efline *curr = &end.lines[end.cursor_y];
					int prevl = prev->length;
					char *merged = realloc(prev->text, prevl + curr->length + 1);
					if(!merged) break;
					memcpy(merged + prevl, curr->text, curr->length);
					merged[prevl + curr->length] = '\0';
					prev->text = merged;
					prev->length = prevl + curr->length;
					free(curr->text);
					for(int i = end.cursor_y; i < end.num_lines - 1; i++){
						end.lines[i] = end.lines[i+1];
					}

					end.num_lines--;
					end.cursor_y--;
					end.cursor_x = prevl;
					ps();
					end.lost = 1;
				}

				break;
			default:
				if(isprint(p)){
					in_char(p);
				}

				break;
		}
	}
}

int main(int argc, char *argv[]){
	enable_rowm();
	ef(argc > 1 ? argv[1] : NULL);
	while(1){
		dscrn();
		pk();
	}

	return 0;
}

;;; @(#) slate-mode.el --- Slate mode
;;; @(#) $Id: slate-mode.el,v 1.7 2005/02/16 21:33:45 water Exp $

;; Copyright (C) 2002 Lee Salzman and Brian T. Rice.

;; Authors: Brian T. Rice <water@tunes.org>
;; Created: August 21, 2002
;; Keywords: languages oop

;; This file is not part of GNU Emacs.

;; Permission is hereby granted, free of charge, to any person
;; obtaining a copy of this software and associated documentation
;; files (the "Software"), to deal in the Software without
;; restriction, including without limitation the rights to use, copy,
;; modify, merge, publish, distribute, sublicense, and/or sell copies
;; of the Software, and to permit persons to whom the Software is
;; furnished to do so, subject to the following conditions:

;; The above copyright notice and this permission notice shall be
;; included in all copies or substantial portions of the Software.

;; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
;; EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
;; MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
;; NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
;; BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
;; ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
;; CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
;; SOFTWARE.

;;; Commentary:

;; Some recognition of method definition syntax is still needed in order to
;; recognize them, which would enable intelligent highlighting of the signature
;; (which is lacking) as well as indentation. Specifically, avoiding the
;; highlighting of `slate-globals-regexp' keywords while being able to
;; make the selector (unary, binary, not just keyword) stands out correctly
;; is desired.

;;; Code:

(defvar slate-mode-abbrev-table nil
  "Abbrev table in use in slate-mode buffers.")
(define-abbrev-table 'slate-mode-abbrev-table ())

;; Templates
;; =========

(defvar slate-template-map
  (let ((map (make-sparse-keymap)))
    (define-key map "m" 'slate-method-template)
    (define-key map "p" 'slate-prototype-template)
    (define-key map "t" 'slate-traits-template)
    map)
  "Slate template creation keys")

(defun slate-prototype-template (namespace proto-name value)
  "Invokes a template, asking for expressions to fill in for a new prototype."
  (interactive
   (list (read-string "Namespace: " "lobby")
	 (read-string "New Name: " "x")
	 (read-string "Value: " "Oddball clone")))
  (insert (format "%s addSlot: #%s valued: %s.\n"
		  namespace proto-name value)))

(defun slate-traits-template (namespace proto-name parent)
  "Invokes a template, asking for expressions to fill in for a new traits."
  (interactive
   (list (read-string "Namespace: " "lobby")
	 (read-string "New Name: " "x")
	 (read-string "Parent: " "Cloneable")))
  (insert (format "%s addPrototype: #%s derivedFrom: {%s}."
		  namespace proto-name parent))
  (save-excursion
    (insert (format "\n\"A %s is a %s.\"\n" proto-name parent))))

(defun slate-method-template (first-arg first-dispatch locals)
  "Invokes a template, asking for expressions to fill in for a new method."
  (interactive
   (list (read-string "First Argument Name: " "_")
	 (read-string "First Dispatch: " "Cloneable traits")
	 (read-string "Locals: " "")))
  (insert (format "\n%s@(%s) " first-arg first-dispatch))
  (save-excursion
    (insert (format "\n[%s\n  \n].\n" (if (string= locals "") ""
					(concat "| " locals " |"))))))

(defvar slate-mode-map
  (let ((map (make-sparse-keymap)))
    (mapcar
     #'(lambda (l)
         (define-key map (car l) (cdr l)))
     `(("\M-\t" . slate-tab)
       ("\t" . slate-reindent)
       ([backspace] . backward-delete-char-untabify)
       ("\n" . slate-newline-and-indent)
       ("\M-\C-a" . slate-begin-of-defun)
       ("\M-\C-f" . slate-forward-sexp)
       ("\M-\C-b" . slate-backward-sexp)
       (":" . slate-colon)
       ;;("@" . slate-dispatch)
       ;;("\C-c\C-d" . slate-category-definition)
       ("\C-cc" . slate-compile)
       ("\C-cd" . slate-macroexpand-region)
       ("\C-ce" . slate-eval-region)
       ("\C-cf" . slate-filein)
       ("\C-cm" . slate)
       ("\C-cp" . slate-print)
       ("\C-cq" . slate-quit)
       ;;("\C-cr" . slate-reeval-region)
       ("\C-cs" . slate-snapshot)
       ("\C-ct" . ,slate-template-map)
       ("\C-cu" . slate-unit-tests)
       ("\C-cw" . slate-workspace)
       ;;("\C-c\C-s" . slate-browse-selectors)
       ;;("\C-x c" . slate-complete-traits)
       ))
    map)
  "Slate mode keymappings")

;; Syntax-Handling
;; ===============

(defconst slate-name-regexp "[A-Za-z][-A-Za-z0-9_:]*[^:]"
  "A regular expression that matches a Slate identifier")

(defconst slate-globals-regexp
  (regexp-opt '("lobby" "True" "False" "Nil" "NoRole" "thisContext"
		"resend" "clone" "here" "it") 'words))

(defconst slate-binop-regexp (concat "\\([-+*/~,;<>=&?]\\{1,3\\}\\|||\\)" slate-name-regexp "\\([-+*/~,;<>=&?]\\{1,3\\}\\|||\\)")
  "A regular expression that matches a Slate binary selector")

(defconst slate-keyword-regexp
  "\\([-A-Za-z0-9_][-A-Za-z0-9_:]*:\\| :[^A-Za-z]\\)"
  "A regular expression that matches a Slate keyword")

(defconst slate-opt-keyword-regexp (concat "&" slate-keyword-regexp)
  "A regular expression that matches a Slate optional-keyword")

(defconst slate-name-chars "A-Za-z0-9"
  "The collection of character that can compose a Slate identifier")

(defconst slate-whitespace-chars " \t\n\f")

(defconst slate-mode-syntax-table
  (let ((table (make-syntax-table)))
    (mapcar
     #'(lambda (l)
         (modify-syntax-entry (car l) (cdr l) table))
     '((?\' . "\"") ; String
       ;(?\"  . "!") ; Comment
       (?+  . "w") ; Binary selector elements...
       (?-  . "w")
       (?*  . "w")
       (?/  . "w")
       (?=  . "w")
       (?%  . "w")
       (?~  . "w")
       (?%  . "w")
       (?\; . "w")
       (?,  . "w")
       (?<  . "w")
       (?>  . "w")
       (?\[ . "(]") ; Block opener
       (?\] . ")[") ; Block closer
       (?\( . "()") ; Parens opener
       (?\) . ")(") ; Parens closer
       (?{  . "(}") ; Array opener
       (?}  . "){") ; Array closer
       (?&  . "'") ; Optional keyword marker
       (?`  . "'") ; Macro character
       (?$  . "'") ; Character literal
       (?#  . "'") ; Symbol
       (?|  . "$") ; Locals
       (?_  . "_") ; Word-element and anonymous argument
       (?:  . "_") ; Keyword marker
       (?\\ . "\\") ; C-like escape
       (?!  . "'") ; A stop in Smalltalk. A type annotation in Slate.
       (?@  . "'") ; Dispatch annotator
       (?^  . "w") ; Return
       (?.  . "."))) ; Statement separator
    table)
  "Slate character types")

(defconst slate-array-face 'bold
  "The face for Slate array braces.")
(defconst slate-keyword-face 'bold
  "The face for keywords in Slate message-sends.")

(defconst bold 'bold
  "Emacs requires this; ugly.")

(defconst italic 'italic)

(defconst slate-font-lock-keywords
  `((,(concat "#[^" slate-whitespace-chars "{}()]+")
     . font-lock-reference-face)	; symbol
    ("[^\\]\"[^\\]\"" . font-lock-comment-face) ; comment
    ("[^#]'\\(.\\|\'\\)*'" . font-lock-string-face) ; string
    ("\$\\(\\\\[ntsbre0avf]\\|.\\)"
     . font-lock-string-face)		; character
    (,(concat "`" slate-binop-regexp)
     . ,(if (boundp 'font-lock-preprocessor-face)
	    'font-lock-preprocessor-face
	  'font-lock-builtin-face)) ; macro call
    (,(concat "`" slate-name-regexp)
     . ,(if (boundp 'font-lock-preprocessor-face)
	    'font-lock-preprocessor-face
	  'font-lock-builtin-face)) ; macro call
    ("[`]+"
     . ,(if (boundp 'font-lock-preprocessor-face)
	    'font-lock-preprocessor-face
	  'font-lock-builtin-face)) ; quotation syntax
    (,slate-opt-keyword-regexp
     . font-lock-variable-name-face)	; optional keywords
    ("#?{" . ,slate-array-face)		; array
    ("}" . ,slate-array-face)
    ("\\(?:_\\|[A-Za-z]+[_A-Za-z0-9]*\\)@+?"
     . font-lock-variable-name-face)	; declaration dispatchings
    (,slate-keyword-regexp . ,slate-keyword-face) ; keyword sends
    ("|[A-Za-z0-9:*!() \n]*|"
     . font-lock-variable-name-face)	; block local slots
    ("\\(:\\|&\\|*\\)[A-Za-z0-9_]+"
     . font-lock-variable-name-face)	; block input slots
    ("!\\([A-Za-z]*\\|\([A-Za-z0-9_ ]*\)\\)"
     . font-lock-type-face)		; type-declaration
    ("\\([.]\\)\\(?:$\\|[^0-9\"]\\)"
     . font-lock-warning-face)		; statement separators
    ("\\(?:[A-Za-z0-9_]* \\)*\\(?:traits\\|derive\\)"
     . font-lock-type-face)		; traits name
    ("\\<\\^\\>" . font-lock-warning-face)	; return
    ("\\<[0-9]+\\>" . font-lock-constant-face) ; integers
    ("\\<[+-]?\\([0-9]+[Rr]\\)?[0-9]+\\([.][0-9]+\\)?\\>"
     . font-lock-constant-face) ; integers and floats
    (,slate-globals-regexp
     . font-lock-keyword-face)		; globals
   )
  "Slate highlighting matchers.")

(defconst slate-indent-amount 2
  "*'Tab size'; used for simple indentation alignment.")

;; Inferior-Mode Support
;; =====================

(require 'comint)

(defconst slate-prompt-regexp "^Slate [0-9]+>"
  "Regexp to match prompts in slate buffer.")

(defconst slate-debug-prompt-regexp "^Debug \[[0-9]+[.][.][0-9]+\]:"
  "Regexp to match prompts in slate buffer.")

(defconst slate-prompt-line-regexp (concat slate-prompt-regexp " .*")
  "Regexp to match the prompt line in the slate buffer.")

(defconst slate-debug-fileref-regexp " @ \\([-A-z/_.]+:[0-9]+\\)$"
  "Regexp to match filename:linenumber in the slate buffer.")

(defvar slate-cmd "slate"
  "The name/path of the VM to be executed for the interactive mode.")

(defvar slate-dir "."
  "The current working directory for the Slate process; this should also be
   set in a preference. It should generally be one's slate installation root.")

(defvar slate-args '()
  "Arguments to pass to the `slate-cmd' launcher. This should be overridden
   in the user's init file.")

(defvar *slate-process* nil
  "The Slate process")

(defvar inferior-slate-buffer-name "*slate*"
  "The Slate interaction buffer name.")

(defvar slate-output-buffer nil
  "Stores accumulating output from the Slate printer.")

(defvar slate-input-queue nil
  "Stores pending inputs to the Slate reader.")

(defconst slate-inf-mode-map (copy-keymap comint-mode-map)
  "The modemap used for interactive Slate sessions.")
(set-keymap-parent slate-inf-mode-map slate-mode-map)

(defvar slate-fileref-keymap (copy-keymap slate-inf-mode-map))
(set-keymap-parent slate-fileref-keymap slate-inf-mode-map)
(define-key slate-fileref-keymap [return] 'slate-follow-name-at-point)
(define-key slate-fileref-keymap [mouse-1] 'slate-follow-name-at-point)

(defconst slate-inf-font-lock-keywords
  `((,slate-prompt-regexp . italic)	; italicized prompt
    (,slate-debug-prompt-regexp . font-lock-warning-face) ; the debug prompt
    ("^\\(Warning\\|Error\\):" . font-lock-warning-face) ; warnings/errors
    ;(,slate-debug-fileref-regexp 1 'link) ; filename/lineno debugger reports
    ("^Slate:" . compilation-info-face) ; VM messages
    ,@slate-font-lock-keywords)
  "Simplified and adjusted highlighting matchers for the interaction.")

(defun slate-inf-mode ()
  "Major mode for interacting Slate subprocesses.

The following commands imitate the usual Unix interrupt and
editing control characters:
\\{slate-inf-mode-map}

Entry to this mode calls the value of `slate-inf-mode-hook' with no arguments,
if that value is non-nil.  Likewise with the value of `shell-mode-hook'.
`slate-inf-mode-hook' is called after `shell-mode-hook'."
  (interactive)
  (kill-all-local-variables)
  (comint-mode)
  (setq comint-prompt-regexp slate-prompt-line-regexp)
  (setq major-mode 'slate-inf-mode)
  (setq mode-name "Slate Interaction")
  (use-local-map slate-inf-mode-map)
  (make-local-variable 'mode-status)
  (set-syntax-table slate-mode-syntax-table)
  (setq font-lock-defaults '(slate-inf-font-lock-keywords))
  (font-lock-mode 1)
  ;(lazy-lock-mode 1)
  (setq slate-output-buffer nil)
  (setq mode-status "Starting Up")
  (run-hooks 'comint-mode-hook 'slate-inf-mode-hook))

(defun slate (&optional cmd)
  "Starting an inferior Slate process.
   Input and output via buffer `inferior-slate-buffer-name'."
  (interactive
   (list (or slate-cmd
	     (read-from-minibuffer "Slate toplevel to run: " slate-cmd))))
  (if (eq major-mode 'slate-inf-mode)
      (apply 'inf-slate slate-cmd slate-args)
    (switch-to-buffer-other-window
     (apply 'inf-slate slate-cmd slate-args)))
  (slate-inf-mode)
  (setq list-buffers-directory slate-dir)
  (setq *slate-process* (get-buffer-process (current-buffer))))

(defun slate-workspace ()
  "Create a scratch workspace buffer `*slate-scratch*' for Slate expressions."
  (interactive)
  (let ((buffer (get-buffer-create "*slate-scratch*")))
    (save-excursion
      (set-buffer buffer)
      (slate-mode)
      (setq mode-name "Slate Workspace")
      (font-lock-mode))
    (pop-to-buffer "*slate-scratch*")))

(defun slate-follow-name-at-point ()
  "Follows a file reference of the form filename:linenumber at/after the point."
  (interactive)
  (push-mark)
  (setq filename
	(save-excursion
	  (skip-chars-forward "^:")
	  (setq end (point))
	  (skip-chars-backward "^ ")
	  (buffer-substring-no-properties (point) end)))
  (setq line-number
	(save-excursion
	  (skip-chars-forward "^:")
	  (forward-char)
	  (string-to-number (buffer-substring-no-properties (point) (progn (forward-word 1) (point))))))
  ;(find-file-at-point)
  (find-file filename)
  (goto-line line-number))

(defun inf-slate (cmd &rest args)
  "Run an inferior Slate process `*slate-process*'.
   Input and output via buffer `inferior-slate-buffer-name'."
  (let ((buffer (get-buffer-create inferior-slate-buffer-name))
	proc status)
    (when (setq proc (get-buffer-process buffer))
      (setq status (process-status proc)))
    (save-excursion
      (set-buffer buffer)
      (cd slate-dir)
      (unless (memq status '(run stop))
	(when proc (delete-process proc))
	(setq proc
	      (if (equal window-system "x")
		  (apply 'start-process
			 cmd buffer
			 "env"
			 (format "TERMCAP=emacs:co#%d:tc=unknown:"
				 (frame-width))
			 "TERM=emacs" "EMACS=t"
			 cmd args)
		(apply 'start-process cmd buffer cmd args)))
	(setq cmd (process-name proc)))
      (goto-char (point-max))
      (set-marker (process-mark proc) (point))
      (set-process-filter proc 'slate-inf-filter)
      ;(set-process-sentinel proc 'slate-inf-sentinel)
      (slate-inf-mode))
    buffer))

(defun slate-handle-command (str)
  (eval (read str)))

(defun slate-accum-command (string)
  (let (where)
    (setq where (string-match "\C-a" string))
    (setq slate-output-buffer
	  (concat slate-output-buffer (substring string 0 where)))
    (if where
	(progn
	  (unwind-protect		;found the delimiter...do it
	      (slate-handle-command slate-output-buffer)
	    (setq slate-output-buffer nil))
	  ;; return the remainder
	  (substring string where))
      ;; we ate it all and didn't do anything with it
      nil)))

;(defun slate-inf-sentinel ())

(defun slate-inf-filter (process string)
  "Make sure that the window continues to show the most recently output
text."
  (let ((where 0) ch command-str)
    (while (and string where)
      (when slate-output-buffer
	(setq string (slate-accum-command string)))
      (when (and string
		 (setq where (string-match "\C-a\\|\C-b" string)))
	(setq ch (aref string where))
	(cond ((= ch ?\C-a)		;strip these out
	       (setq string (concat (substring string 0 where)
				    (substring string (1+ where)))))
	      ((= ch ?\C-b)		;start of command
	       (setq slate-output-buffer "") ;start this off
	       (setq string (substring string (1+ where)))))))
    
    (save-excursion
      (set-buffer (process-buffer process))
      (goto-char (point-max))
      (when string
	(setq mode-status "Idle")
	(insert string))
      ; Handle debugger file references:
      (save-excursion
	(goto-char (point-min))
	(let (fileref-end)
	  (while (setq fileref-end (re-search-forward slate-debug-fileref-regexp nil t))
	    (let* ((fileref-overlay (make-overlay (match-beginning 1) fileref-end))
		   (fileref (match-string 1))
		   (lineno-begin (re-search-forward ":" nil t)))
	      (overlay-put fileref-overlay 'face 'link)
	      (overlay-put fileref-overlay 'mouse-face 'highlight)
	      (overlay-put fileref-overlay 'keymap slate-fileref-keymap)
	      (replace-match "" t t)))))
      (when (process-mark process)
	(set-marker (process-mark process) (point-max)))))
  (let ((buf (current-buffer)))
    (set-buffer (process-buffer process))
    (goto-char (point-max)) (sit-for 0)
    (set-window-point (get-buffer-window (current-buffer)) (point-max))
    (set-buffer buf)))

(defvar slate-interactor-mode-map
  (let ((map (copy-keymap slate-mode-map)))
    (mapcar
     #'(lambda (l) (define-key map (car l) (cdr l)))
     '(("\C-m"      . 'comint-send-input)
       ("\C-c\C-d"  . comint-delchar-or-maybe-eof)
       ("\C-c\C-u"  . comint-kill-input)
       ("\C-c\C-c"  . comint-interrupt-subjob)
       ("\C-c\C-z"  . comint-stop-subjob)
       ("\C-c\C-\\" . comint-quit-subjob)
       ("\C-c\C-o"  . comint-kill-output)
       ("\C-c\C-r"  . comint-show-output)))
    map)
  "Keymap for controlling the Slate listener")

(defun slate-ensure-running ()
  (unless (comint-check-proc inferior-slate-buffer-name)
    (slate)))

(defun slate-eval-region (start end)
  "Send the current region to the inferior Slate process. A stop character (a period) will be added to the end if necessary."
  (interactive "r")
  (slate-ensure-running)
  (save-excursion
    (goto-char end)
    (slate-backward-whitespace)
    (comint-send-region inferior-slate-buffer-name start (point))
    (process-send-string
     inferior-slate-buffer-name
     (if (and (>= (point) 2) (equal (preceding-char) ?.)) "\n" ".\n"))
    (display-buffer inferior-slate-buffer-name t)))

(defun slate-macroexpand-region (start end)
  "Send the current region to the inferior Slate process, quoted, with a macroExpand call to get the macroExpand'd result."
  (interactive "r")
  (slate-ensure-running)
  (save-excursion
    (goto-char end)
    (slate-backward-whitespace)
    (process-send-string
     inferior-slate-buffer-name
     "`(")
    (comint-send-region inferior-slate-buffer-name start (point))
    (process-send-string
     inferior-slate-buffer-name
     ") macroExpand.\n")
    (display-buffer inferior-slate-buffer-name t)))

(defun slate-print (start end)
  "Performs `slate-eval-region' on the current region and inserts the output
into the current buffer after the cursor."
  (interactive "r")
  (let ((orig-buffer (current-buffer)))
    (slate-ensure-running)
    (set-process-filter
     *slate-process*
     (lambda (proc string) (insert string orig-buffer))))
  (save-excursion
    (goto-char end)
    (slate-backward-whitespace)
    (comint-send-region inferior-slate-buffer-name start (point))
    (process-send-string
     inferior-slate-buffer-name
     (if (and (>= (point) 2) (equal (preceding-char) ?.)) "\n" ".\n")))
  (set-process-filter *slate-process* 'slate-inf-filter))

(defun slate-quit ()
  "Terminate the Slate session and associated process."
  (interactive)
  (setq mode-status "Quitting")
  (process-send-string inferior-slate-buffer-name "quit.\n"))

(defun slate-snapshot (filename)
  "Save a Slate snapshot."
  (interactive "FSnapshot name to save:")
  (setq mode-status "Saving")
  (process-send-string inferior-slate-buffer-name
		       (format "Image saveNamed: '%s'.\n"
			       (expand-file-name filename))))

'(defun slate-compile (filename)
  "Do a compileFileNamed: on FILENAME."
  (interactive "FSlate file to compile: ")
  (slate-ensure-running)
  (setq mode-status "Compiling")
  (process-send-string inferior-slate-buffer-name
		       (format "lobby compileFileNamed: '%s'.\n"
			       (expand-file-name filename))))

(defun slate-filein (filename)
  "Do a load: on FILENAME."
  (interactive "FSlate file to load: ")
  (slate-ensure-running)
  (setq mode-status "Loading")
  (process-send-string inferior-slate-buffer-name
		      (format "load: '%s'.\n" (expand-file-name filename))))

(defun slate-unit-tests (filename)
  "Load the unit-test file for the current file and run the tests."
  (interactive "FUnit-test file to load: ")
  (slate-filein filename)
  (setq mode-status "Running tests")
  (process-send-string inferior-slate-buffer-name
		      "load: '%s'.\n" (expand-file-name filename))
  (process-send-string inferior-slate-buffer-name
		      "Tests CurrentUnit testSuite.\n"))

(defun slate-send (str &optional mode)
  (let (temp-file buf old-buf)
    (setq temp-file (concat temporary-file-directory (make-temp-name "slate")))
    (save-excursion
      (setq buf (get-buffer-create " zap-buffer "))
      (set-buffer buf)
      (erase-buffer)
      (princ str buf)
      (write-region (point-min) (point-max) temp-file nil 'no-message))
    (kill-buffer buf)
    ;; this should probably be conditional
    (save-window-excursion (slate slate-args))
    (setq old-buf (current-buffer))
    (setq buf (process-buffer *slate-process*))
    (pop-to-buffer buf)
    (when mode
      (setq mode-status mode))
    (goto-char (point-max))
    (newline)
    (pop-to-buffer old-buf)
    (process-send-string *slate-process*
			   (format "'%s' fileIn.\n" temp-file))))

;; Imenu Support
;; =============
;; INCOMPLETE

(defconst slate-imenu-generic-expression
  '(("Traits" (format "^.*add[A-Za-z]*Slot: #\\(%s\\) valued: .* derive"
		      slate-name-regexp) 1)
    ("Traits" (format "^.*addPrototype: #\\(%s\\) derivedFrom: {.*}\."
		      slate-name-regexp) 1)
    ("Methods" "\\(^.*@.*\\)\\(\[\\|$\\)" 1) ; Matches the whole signature.
    ))

'(defun slate-find-next-decl ()
  "Find the name, position and type of the declaration at or after
point.  Returns `((name . (start-position . name-position)) . type)'
if one exists and nil otherwise.  The start-position is at the start
of the declaration, and the name-position is at the start of the name
of the declaration.  The name is a string, the positions are buffer
positions and the type is one of the symbols `traits' or `method'."
  (let (name type name-pos
	     start end
	     (orig-table (syntax-table)))
    ;; Change to declaration scanning syntax.
    (set-syntax-table slate-ds-syntax-table)
    ;; Stop when we are at the end of the buffer or when a valid
    ;; declaration is grabbed.
    (while (not (or (eobp) name))
      ;; Move forward to next declaration at or after point.

	;; If we did not manage to extract a name, cancel this
	;; declaration (eg. when line ends in "=> ").
	(if (string-match "^[ \t]*$" name) (setq name nil))
	(setq type 'instance)))
      ;; Move past start of current declaration.
      (goto-char end)
    ;; Replace syntax table.
    (set-syntax-table orig-table)
    ;; If we have a valid declaration then return it, otherwise return
    ;; nil.
    (if name
	(cons (list* name (copy-marker start t) (copy-marker name-pos t)) type)
      nil))

'(defun slate-create-imenu-index ()
  "Finds `imenu' declarations in Slate mode. Finds all Traits (and eventually
method-definitions in a Slate source file for index inclusion."
  (let*
      ((index-alist '())
       (index-traits-alist '())
       (index-methods-alist '())
       (bufname (buffer-name))
       (progress-divisor (max 1 (/ (point-max) 100)))
       result)
        (goto-char (point-min))
    ;; Loop forwards from the beginning of the buffer through the
    ;; starts of the top-level declarations.
    (while (< (point) (point-max))
      (message "Scanning declarations in %s... (%3d%%)" bufname
	       (/ (point) progress-divisor))
      ;; Grab the next declaration.
      (setq result (slate-generic-find-next-decl))
      (when result
	;; If valid, extract the components of the result.
	(let* ((name-posns (first result))
	       (name (first name-posns))
	       (posns (rest name-posns))
	       (start-pos (first posns))
	       (type (rest result))
	       ;; Place `(name . start-pos)' in the correct alist.
	       (alist (cond
		       ((eq type 'traits) 'index-traits-alist)
		       ((eq type 'method) 'index-methods-alist))))
	  (set alist (cons (cons name start-pos) (eval alist))))))
    ;; Now sort all the lists, label them, and place them in one list.
    (message "Sorting declarations in %s..." bufname)
    (when index-traits-alist
      (push `("Traits" . ,index-traits-alist)) index-alist)
    (when index-methods-alist
      (push `("Methods" . ,index-methods-alist)) index-alist)
    (message "Sorting declarations in %s...done" bufname)
    ;; Return the alist.
    index-alist))

;; Indentation
;; ===========

;; Basic utilities: rely on only basic Emacs functions.

(defun slate-comment-indent ()
  "This is used by `indent-for-comment' to decide how much to indent a comment
in Slate code based on its context."
  (if (looking-at "^\"")
      0				;Existing comment at bol stays there.
      (save-excursion
        (skip-chars-backward " \t")
        (max (1+ (current-column))	;Else indent at comment column
             comment-column))))	; except leave at least one space.

(defun slate-indent-to-column (col)
  (save-excursion
    (beginning-of-line)
    (indent-line-to col))
  (when (< (current-column) col)
    (move-to-column col)))

(defun slate-current-column ()
  "Returns the current column of the given line, regardless of narrowed buffer."
  (save-restriction
    (widen)
    (current-column)))

(defun slate-previous-nonblank-line ()
  (forward-line -1)
  (while (and (not (bobp))
	      (looking-at "^[ \t]*$"))
    (forward-line -1)))

(defun slate-in-string ()
  "Returns non-nil delimiter as a string if the current location is
actually inside a string or string like context."
  (let (state)
    (setq state (parse-partial-sexp (point-min) (point)))
    (and (nth 3 state)
	 (char-to-string (nth 3 state)))))

(defun slate-white-to-bolp ()
  "Returns T if from the current position to beginning of line is whitespace."
  (let (done is-white line-start-pos)
    (save-excursion
      (save-excursion
	(beginning-of-line)
	(setq line-start-pos (point)))
      (while (not done)
	(unless (bolp)
	  (skip-chars-backward " \t"))
	(cond ((bolp)
	       (setq done t)
	       (setq is-white t))
	      ((equal (char-after (1- (point))) ?\")
	       (backward-sexp)
	       (when (< (point) line-start-pos) ;comment is multi line
		 (setq done t)))
	      (t
	       (setq done t))))
      is-white)))

(defun slate-backward-comment ()
  "Moves to the beginning of the containing comment, or
the end of the previous one if not in a comment."
  (search-backward "\"")		;find its start
  (while (equal (preceding-char) ?\\)	;skip over escaped ones
    (backward-char 1)
    (search-backward "\"")))

;; Basic utilities that use `slate-mode' variables.

(defun slate-forward-whitespace ()
  "Skip white space and comments forward, stopping at end of buffer
or non-white space, non-comment character."
  (while (looking-at (concat "[" slate-whitespace-chars "\"]"))
    (skip-chars-forward slate-whitespace-chars)
    (when (equal (following-char) ?\")
      (forward-sexp))))

(defun slate-backward-whitespace ()
  "Like `slate-forward-whitespace' only going towards the start of the buffer."
  (while (progn (skip-chars-backward slate-whitespace-chars)
		(equal (preceding-char) ?\"))
    (backward-sexp)))

(defun slate-tab ()
  "This gets called when the user hits [tab] in a `slate-mode' buffer."
  (interactive)
  (let (col)
    ;; round up, with overflow
    (setq col (* (/ (+ (current-column) slate-indent-amount)
		    slate-indent-amount)
		 slate-indent-amount))
    (indent-to-column col)))

;; Higher-level utilities calling `slate-mode' functions.

(defun slate-forward-sexp (&optional n)
  "Move forward N Slate expressions."
  (interactive "p")
  (unless n (setq n 1))
  (cond ((< n 0)
         (slate-backward-sexp (- n)))
        ((null parse-sexp-ignore-comments)
         (forward-sexp n))
        (t
         (while (> n 0)
           (slate-forward-whitespace)
           (forward-sexp)
           (decf n)))))

(defun slate-backward-sexp (&optional n)
  "Move backward N Slate expressions."
  (interactive "p")
  (unless n (setq n 1))
  (cond ((< n 0)
         (slate-forward-sexp (- n)))
        ((null parse-sexp-ignore-comments)
         (backward-sexp n))
        (t
         (while (> n 0)
           (slate-backward-whitespace)
           (backward-sexp)
           (decf n)))))

(defun slate-calculate-indent ()
  "The core calculations for indentation."
  (let (indent-amount start-of-line state (parse-sexp-ignore-comments t))
    (save-excursion
      (save-restriction
	(widen)
	(narrow-to-region (point-min) (point)) ;only care about what's before
	(setq state (parse-partial-sexp (point-min) (point)))
	(cond ((equal (nth 3 state) ?\") ;in a comment
	       (save-excursion
		 (slate-backward-comment)
		 (setq indent-amount (1+ (current-column)))))
	      ((equal (nth 3 state) ?')	;in a string
	       (setq indent-amount 0)))
	(when indent-amount
	  (return-from slate-calculate-indent indent-amount))
	(slate-narrow-to-method)
	(beginning-of-line)
	(setq state (parse-partial-sexp (point-min) (point)))
;	(slate-narrow-to-paren state)
	(slate-backward-whitespace)
	(cond ((bobp)	;must be first statement in block or exp
	       (if (nth 1 state)	;within a paren exp
		   (setq indent-amount (+ (slate-current-column)
					  slate-indent-amount))
		 ;; we're top level
		 (setq indent-amount slate-indent-amount)))
	      ((equal (nth 0 state) 0) ;at top-level
	       (beginning-of-line)
	       (slate-forward-whitespace)
	       (setq indent-amount (slate-current-column)))
	      ((equal (preceding-char) ?.) ;at end of statement
	       (slate-find-statement-begin)
	       (setq indent-amount (slate-current-column)))
	      ((memq (preceding-char) '(?| ?\[))
	       (backward-char)
	       (skip-chars-backward "^\[")
	       (slate-backward-whitespace)
	       (setq indent-amount (+ (slate-current-column)
				      slate-indent-amount)))
	      ((equal (preceding-char) ?:)
	       (beginning-of-line)
	       (slate-forward-whitespace)
	       (setq indent-amount (+ (slate-current-column)
				      slate-indent-amount)))
	      (t			;must be a statement continuation
	       (slate-find-statement-begin)
	       (setq indent-amount (+ (slate-current-column)
				      slate-indent-amount)))))
      indent-amount)))

(defun slate-indent-line ()
  "Sees if the current line is a new statement, in which case indent the same
as the previous statement, or determine by context. If not the start of a new
statement, the start of the previous line is used, except if that starts a
new line, in which case it indents by `slate-indent-amount'."
  (let (indent-amount is-keyword)
    (save-excursion
      (beginning-of-line)
      (slate-forward-whitespace)
      (when (looking-at "[A-Za-z][A-Za-z0-9]*:") ;indent for colon
	(let ((parse-sexp-ignore-comments t))
	  (beginning-of-line)
	  (slate-backward-whitespace)
	  (unless (memq (preceding-char) '(?. ?| ?\[ ?\( ?\{))
	    (setq is-keyword t)))))
    (if is-keyword
	(slate-indent-for-colon)
      (setq indent-amount (slate-calculate-indent))
      (slate-indent-to-column indent-amount))))

(defun slate-reindent ()
  (interactive)
  ;; Still loses if at first character on line
  (slate-indent-line))

(defun slate-newline-and-indent ()
  (interactive "p")
  (newline)
  (slate-indent-line))

(defun slate-begin-of-defun ()
  "Skip to the beginning of the current method.
If already at the beginning of a method, skips to the beginning of the
previous one."
  (interactive)
  (let ((parse-sexp-ignore-comments t) here delim start)
    (setq here (point))
    (while (and (search-backward "@" nil 'to-end)
		(setq delim (slate-in-string)))
      (search-backward delim))
    (setq start (point))
    (beginning-of-line)
    (slate-forward-whitespace)
    ;; check to see if we were already at the start of a method
    ;; in which case, the semantics are to go to the one preceding
    ;; this one
    (when (and (= here (point))
	       (/= start (point-min)))
      (goto-char start)
      (slate-backward-whitespace)	;may be at ! "foo" !
      (when (equal (preceding-char) ?@)
;	(backward-char)
	(beginning-of-line)
	(slate-forward-whitespace)
	(slate-backward-sexp 1))
      (slate-begin-of-defun))))		;and go to the next one

(defun slate-find-statement-begin ()
  "Leaves the point at the first non-blank, non-comment character of a new
statement.  If beginning of buffer is reached, then the point is left there.
This routine only will return with the point pointing at the first non-blank
on a line; it won't be fooled by multiple statements on a line into stopping
prematurely.  Also, goes to start of method if we started in the method
selector."
  (let (start ch)
    (when (equal (preceding-char) ?.)	;if we start at eos
      (backward-char 1))		;we find the begin of THAT stmt
    (while (and (null start) (not (bobp)))
      (slate-backward-whitespace)
      (cond ((equal (setq ch (preceding-char)) ?.)
	     (let (saved-point)
	       (setq saved-point (point))
	       (slate-forward-whitespace)
	       (if (slate-white-to-bolp)
		   (setq start (point))
		 (goto-char saved-point)
		 (slate-backward-sexp 1))))
	    ((equal ch ?^)		;HACK -- presuming that when we back
					;up into a return that we're at the
					;start of a statement
	     (backward-char 1)
	     (setq start (point)))
	    ((equal ch ?\[)
	     (setq start (point)))
	    ((equal ch ?|)
	     (backward-char 1)
	     (skip-chars-backward "^\[")
	     (slate-backward-whitespace)
	     (setq start (point)))
	    (t
	     (slate-backward-sexp 1))))
    (unless start
      (goto-char (point-min))
      (slate-forward-whitespace)
      (setq start (point)))
    start))

(defun slate-narrow-to-paren (state)
  "Narrows the region to between point and the closest previous open paren.
Actually, skips over any block parameters, and skips over the whitespace
following on the same line."
  (let ((paren-addr (nth 1 state))
	start c done)
    (when paren-addr
      (save-excursion
	(goto-char paren-addr)
	(setq c (following-char))
	(cond ((memq c '(?\( ?\{))
	       (setq start (1+ (point))))
	      ((eq c ?\[)
	       (setq done nil)
	       (forward-char 1)
	       (skip-chars-forward " \t\n")
	       (when (eq (following-char) ?|) ;opens a block header
		 (forward-char 1) ;skip vbar
		 (while (not done)
		   (skip-chars-forward " \t")
		   (setq c (following-char))
		   (cond ((eq c ?|)
			  (forward-char 1) ;skip vbar
			  (skip-chars-forward " \t")
			  (setq done t)) ;done
			 ((eq c ?:)
			  (skip-chars-forward "A-Za-z0-9" 1)) ;skip input slot
			 ((eq c ?\n)
			  (setq done t)) ;don't accept line-wraps
			 (t
			  (skip-chars-forward "A-Za-z0-9"))))) ;skip local slot
	       (setq start (point)))))
      (narrow-to-region start (point)))))

(defun slate-at-method-begin ()
  "Return T if at the beginning of a block, otherwise nil"
  (let ((parse-sexp-ignore-comments t))
    (when (bolp)
      (save-excursion
        (slate-backward-whitespace)
        (memq (preceding-char) '(?| ?\[))))))

(defun slate-colon ()
  "Possibly reindents a line when a colon is typed.
If the colon appears on a keyword that's at the start of the line (ignoring
whitespace, of course), then the previous line is examined to see if there
is a colon on that line, in which case this colon should be aligned with the
left most character of that keyword.  This function is not fooled by nested
expressions."
  (interactive)
  (self-insert-command 1)
  (let (needs-indent state (parse-sexp-ignore-comments t))
    (setq state (parse-partial-sexp (point-min) (point)))
    (unless (nth 3 state)		;unless in string or comment
      (save-excursion
	(skip-chars-backward slate-name-chars)
	(when (and (looking-at slate-name-regexp)
		   (not (slate-at-method-begin)))
	  (setq needs-indent (slate-white-to-bolp))))
      (when needs-indent
	(slate-indent-for-colon)))))

(defun slate-indent-for-colon ()
  "Called only for lines which look like `<whitespace>foo:'."
  (let (indent-amount c state done default-amount start-line
		      (parse-sexp-ignore-comments t))
    (save-excursion
      (save-restriction
	(widen)
	(slate-narrow-to-method)
	(beginning-of-line)
	(setq state (parse-partial-sexp (point-min) (point)))
	(narrow-to-region (point-min) (point))
	(setq start-line (point))
	(slate-backward-whitespace)
	(cond
	 ((bobp)
	  (setq indent-amount (slate-current-column)))
	 ((or (eq (setq c (preceding-char)) ?\|)
	      (eq c ?\[))		; method header before
	  (skip-chars-backward "^\[")
	  (setq indent-amount slate-indent-amount))
	 ((eq c ?.)	; stmt end, indent like it
	  (slate-find-statement-begin)
	  (setq indent-amount (slate-current-column)))
	 (t				;could be a winner
	  (slate-find-statement-begin)
	  ;; we know that since we weren't at bobp above after backing
	  ;; up over white space, and we didn't run into a ., we aren't
	  ;; at the beginning of a statement, so the default indentation
	  ;; is one level from statement begin
	  (setq default-amount
		(+ (slate-current-column) ;just in case
		   slate-indent-amount))
	  ;; might be at the beginning of a method (the selector), decide
	  ;; this here
	  (unless (looking-at slate-keyword-regexp)
	    ;; not a method selector
	    (while (and (not done) (not (eobp)))
	      (slate-forward-sexp 1) ;skip over receiver
	      (slate-forward-whitespace)
	      (unless (and indent-amount ;pick up only first one
			   (not (looking-at slate-keyword-regexp)))
		(setq indent-amount (slate-current-column)))))
	  (unless indent-amount
	    (setq indent-amount default-amount))))))
    (when indent-amount
      (slate-indent-to-column indent-amount))))

(defun slate-narrow-to-method ()
  "Narrows the buffer to the contents and signature of the method."
  ; TODO: Make sure the signature plus optional head comment is included.
  (let ((end (point))
	(parse-sexp-ignore-comments t)
	handled)
    (save-excursion
      (slate-begin-of-defun)
      (if (looking-at "[A-Za-z]")	;either unary or keyword msg
	  ;; or maybe an immediate expression...
	  (progn
	    (forward-sexp)
	    (if (equal (following-char) ?:) ;keyword selector
		(progn			;parse full keyword selector
		  (backward-sexp 1)	;setup for common code
		  (slate-forward-keyword-selector))
	      ;; else maybe just a unary selector or maybe not
	      ;; see if there's stuff following this guy on the same line
	      (let (here eol-point)
		(setq here (point))
		(end-of-line)
		(setq eol-point (point))
		(goto-char here)
		(slate-forward-whitespace)
		(if (< (point) eol-point) ;if there is, we're not a method
		    (beginning-of-line)
		  (goto-char here)	;else we're a unary method (guess)
		  ))))
          
          ;; this must be a binary selector, or a temporary
          (when (equal (following-char) ?|)
	    (end-of-line)	;could be temporary
	    (slate-backward-whitespace)
	    (when (equal (preceding-char) ?|)
	      (setq handled t))
	    (beginning-of-line))
          (unless handled
	    (skip-chars-forward (concat "^" slate-whitespace-chars))
	    (slate-forward-whitespace)
	    (skip-chars-forward slate-name-chars))) ;skip over operand
      (slate-forward-whitespace)
      (if (equal (following-char) ?|)	;scan for temporaries
	  (progn
	    (forward-char)		;skip over |
	    (slate-forward-whitespace)
	    (while (and (not (eobp))
			(looking-at "[A-Za-z]"))
	      (skip-chars-forward slate-name-chars)
	      (slate-forward-whitespace))
	    (when (and (equal (following-char) ?|) ;if a matching | as a temp
		       (< (point) end))	;and we're after the temps
	      (narrow-to-region (1+ (point)) end) ;we limit the buffer
	      ))
	(when (< (point) end)
	  (narrow-to-region (point) end))))))

(defun slate-forward-keyword-selector ()
  "Starting on a keyword, this function skips forward over a keyword selector.
It is typically used to skip over the actual selector for a method."
  (interactive)
  (let (done)
    (while (not done)
      (if (not (looking-at "[A-Za-z_]"))
	  (setq done t)
	(skip-chars-forward slate-name-chars)
	(if (equal (following-char) ?:)
	    (progn
	      (forward-char)
	      (slate-forward-sexp)
	      (slate-forward-whitespace))
	  (setq done t)
	  (backward-sexp))))))

(defun slate-collect-selector ()
  "Point is stationed inside or at the beginning of the selector in question.
This function computes the Slate selector (unary, binary, or keyword) and
returns it as a string.  Point is not changed."
  (save-excursion
    (let (start selector done ch
		(parse-sexp-ignore-comments t))
      (skip-chars-backward (concat "^" "\"" slate-whitespace-chars))
      (setq start (point)) ;back to whitespace
      (if (looking-at slate-name-regexp)
	  (progn			;maybe unary, maybe keyword
	    (skip-chars-forward slate-name-chars)
	    (if (equal (following-char) ?:)	;keyword?
		(progn
		  (forward-char 1)
		  (setq selector (buffer-substring start (point)))
		  (setq start (point))
		  (while (not done)
		    (slate-forward-whitespace)
		    (setq ch (following-char))
		    (cond ((memq ch '(?@ ?\] ?\) ?.)) ;stop at end of expr
			   (setq done t))
			  ((equal ch ?:) ;add the next keyword
			   (forward-char 1)
			   (setq selector
				 (concat selector
					 (buffer-substring start (point)))))
			  (t
			   (setq start (point))
			   (slate-forward-sexp 1)))))
	      (setq selector (buffer-substring start (point)))))
	(skip-chars-forward (concat "^\"" slate-whitespace-chars))
	(setq selector (buffer-substring start (point))))
      selector)))

(defun slate-collect-signature ()
  "Similar to slate-collect-selector except that it looks for dispatching
annotations. It returns the selector string and the names of the arguments in
a list. Note that the first argument must be found by searching backwards."
  (save-excursion
    (let (start selector done ch arg-names
		(parse-sexp-ignore-comments t))
      (skip-chars-backward (concat "^" "\"" slate-whitespace-chars))
      (setq start (point))
      (if (looking-at slate-name-regexp)
	  (progn			;maybe unary, maybe keyword
	    (skip-chars-forward slate-name-chars)
	    (if (equal (following-char) ?:)	;keyword?
		(progn
		  (forward-char 1)
		  (setq selector (buffer-substring start (point)))
		  (setq start (point))
		  (while (not done)
		    (slate-forward-whitespace)
		    (setq ch (following-char))
		    (cond ((memq ch '(?@ ?\] ?\) ?.))
			   (setq done t))
			  ((equal ch ?:)
			   (forward-char 1)
			   (setq selector
				 (concat selector
					 (buffer-substring start (point)))))
			  (t
			   (setq start (point))
			   (slate-forward-sexp 1)))))
	      (setq selector (buffer-substring start (point)))))
	(skip-chars-forward (concat "^" ?\" slate-whitespace-chars))
	(setq selector (buffer-substring start (point))))
      selector)))

(defun slate-mode ()
  "Major mode for editing Slate code.
Type `M-x slate' to open a Slate interaction area.
Notes:
`slate-mode-hook' is activated on entering the mode.
\\{slate-mode-map}"
  (interactive)
  (kill-all-local-variables)
  (setq major-mode 'slate-mode
        mode-name "Slate")
  (use-local-map slate-mode-map)
  (set-syntax-table slate-mode-syntax-table)
  (setq font-lock-defaults '(slate-font-lock-keywords))
  (mapc
   (lambda (l)
     (set (make-local-variable (car l)) (cdr l)))
   '((paragraph-start              . "^\\|$")
     (paragraph-separate           . "[ ^\\|]*$")
     (paragraph-ignore-fill-prefix . t)
     (indent-line-function         . slate-indent-line)
     (require-final-newline        . t)
     (comment-start                . "\"")
     (comment-end                  . "\"")
     (comment-column               . 32)
     (comment-start-skip           . "\" *")
     (comment-indent-function      . slate-comment-indent)
     (parse-sexp-ignore-comments   . nil)
     (local-abbrev-table           . slate-mode-abbrev-table)
     (imenu-generic-expression     . slate-imenu-generic-expression)))
  (setq font-lock-verbose t)
  (run-hooks 'slate-mode-hook))

(provide 'slate-mode)

(add-to-list 'auto-mode-alist '("\\.slate" . slate-mode))

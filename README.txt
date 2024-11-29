Visual Text Editor - README

1. 프로젝트 소개
Windows와 Linux, Mac 환경에서 편집, 저장, 찾기 등의 기능을 갖춘 메모리 에디터

2. 주요 기능
- 기본 텍스트 편집 ( 입력, 삭제, 줄 바꾸기 )
- 파일 열기 및 저장
- 커서 이동
- 검색 기능 ( 하이라이트 )

3. 단축키
- Ctrl+S : 파일 저장
- Ctrl+Q : 프로그램 종료 ( 비저장 시 재확인 )
- Ctrl+F : 검색 모드
- 화살표 키 : 커서 이동
- Home/End : 줄의 시작/끝으로 이동
- Page Up/Down : 페이지 단위로 이동 ( 현재 화면 행 사이즈 만큼 이동 )

4. 사용 방법
4.1 프로그램 실행
- Windows: viva.exe [파일명]
- Linux: ./viva [파일명]

4.2 텍스트 편집
- 일반적인 키보드 입력으로 텍스트를 입력
- 화면 사이즈보다 긴 문자열이 입력될 경우 새로운 구조체로 만들어져서 개행되어 표시
- Backspace로 문자를 삭제
- 해당 행에 커서 위치에서 지울게 없으면 이전 행으로 이동 ( 커서 뒤에 문자열이 있으면 이전 행과 합침 )
- Enter로 새 줄을 삽입

4.3 파일 저장
- Ctrl+S를 눌러 현재 파일을 저장
- 파일명이 지정되지 않은 경우, 저장할 파일명을 입력하라는 프롬프트가 표시

4.4 검색 기능
- Ctrl+F를 눌러 검색 모드를 활성화
- 검색어를 입력한 후 Enter를 누름
- 검색 결과 전부 하이라이트
- 검색 후 기능:
  - 오른쪽 화살표: 다음 검색 결과로 이동
  - 왼쪽 화살표: 이전 검색 결과로 이동
  - Enter : 검색 모드 종료 ( 현재 보고 있는 검색 결과의 행에 위치 하기에 바로 수정 가능 )
  - ESC : 검색 모드 종료 ( 커서가 검색 모드 활성화 되기 전의 위치로 돌아감 )

5. 화면 구성
- 주 편집 영역: 텍스트를 입력하고 편집하는 공간
- 상태 바: 파일명, 총 줄 수, 현재 커서 위치 등 정보 표시
- 메시지 바: 도움말과 상태 메시지 표시

6. 기술
- 링크드 리스트 구조로 텍스트 관리
- 동적 메모리 할당
- ncurses/PDCurses 라이브러리 사용
  초기화 및 종료
  - initscr(): 화면 초기화 및 ncurses 모드 시작
  - endwin(): ncurses 모드 종료 및 터미널 상태 복원
  입력 모드 설정
  - raw(): 입력 버퍼링을 비활성화하여 키 입력 처리
  - keypad(stdscr, TRUE): 특수키 활성화
  화면 제어
  - clear(): 화면 지움
  - refresh(): 화면 갱신
  - move(y, x): 커서를 지정한 위치로 이동
  문자열 출력
  - mvaddstr(y, x, str): 지정한 위치에 문자열 출력
  - mvaddnstr(y, x, str, n): 지정한 위치에 n개의 문자 출력
  속성 설정
  - attron(attr): 지정된 속성 활성화
  - attroff(attr): 지정된 속성 비활성화
  색상 처리
  - start_color(): 색상 모드 초기화
  - init_pair(pair, fg, bg): 색상 쌍 정의
  창 크기
  - getmaxyx(stdscr, rows, cols): 현재 터미널 창의 크기
  입력처리
  - getch(): 키보드 입력 문자 받기
  스크롤
  - scrollok(stdscr, TRUE): 스크롤 활성화

7. 빌드 정보
- 컴파일러: GCC
- 빌드 명령어: make
- 정리 명령어: make clean ( 빌드된 파일 정리 )

8. 주의 사항
8.1 Linux 환경
- ncurses 라이브러리가 시스템에 설치되어 있어야 함
- 대부분의 Linux 배포판에는 기본적으로 설치되어 있지만, 없는 경우 설치해야 함
- 설치 명령어
  `sudo apt-get install libncurses5-dev libncursesw5-dev`

8.2 Mac 환경
- ncurses 라이브러리가 시스템에 설치되어 있어야 함
- 설치 명령어
  `brew install ncurses`

8.3 Windows 환경
- MinGW로 gcc, make 설치
- PDCurses 라이브러리가 필요. Makefile에서 자동으로 DLL 파일을 복사함
- 라이브러리 파일(pdcurses.dll)이 실행 파일과 같은 디렉토리에 있어야 함
- 파일 다운로드
  `git clone https://github.com/wmcbrine/PDCurses.git`

